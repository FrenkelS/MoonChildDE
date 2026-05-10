#include <conio.h>
#include <cstdint>
#include <cstdio>
#include <cstring>
#include <dos.h>
#include <string.h>
#include <time.h>

#include "Audio.h"
#include "Game.h"
#include "MoviePlayer.h"
#include "Util.h"
#include "vesa.h"

#define _IN_MAIN
#include "frm_int.hpp"

#include "moonchild/globals.hpp"
#include "moonchild/mc.hpp"
#include "moonchild/prefs.hpp"


#if defined __DJGPP__
#include <dpmi.h>
#include <go32.h>
#include <sys/nearptr.h>

//DJGPP doesn't inline inp, outp and outpw,
//but it does inline inportb, outportb and outportw
#define inp(port)       inportb(port)
#define outp(port,data) outportb(port,data)

#define __far
#define __interrupt
#define _Noreturn [[noreturn]]

#define replaceInterrupt(OldInt,NewInt,vector,handler)				\
_go32_dpmi_get_protected_mode_interrupt_vector(vector, &OldInt);	\
																	\
NewInt.pm_selector = _go32_my_cs(); 								\
NewInt.pm_offset = (uint32_t)handler;								\
_go32_dpmi_allocate_iret_wrapper(&NewInt);							\
_go32_dpmi_set_protected_mode_interrupt_vector(vector, &NewInt)

#define restoreInterrupt(vector,OldInt,NewInt)						\
_go32_dpmi_set_protected_mode_interrupt_vector(vector, &OldInt);	\
_go32_dpmi_free_iret_wrapper(&NewInt);
#elif defined __WATCOMC__
#define __djgpp_nearptr_enable()
#define __djgpp_conventional_base 0

#define __far
#define _Noreturn __declspec(aborts)

#define replaceInterrupt(OldInt,NewInt,vector,handler)	\
OldInt = _dos_getvect(vector);							\
_dos_setvect(vector, handler)

#define restoreInterrupt(vector,OldInt,NewInt)	_dos_setvect(vector,OldInt)
#else
#error unsupported compiler
#endif


_Noreturn void I_Error(const char *error, ...);


namespace {

const int screenWidth = 640;
const int screenHeight = 480;


MoviePlayer *moviePlayer = nullptr;

uint8_t *pixelBuffer = nullptr;

bool movieFinishedNaturally = false;
bool movieDoneSignal = false;

void onMovieDone(bool naturalEnd, void *userData) {
  (void)userData;
  movieFinishedNaturally = naturalEnd;
  movieDoneSignal = true;
}


static int myargc;
static char **myargv;


static bool M_CheckParm(char *check) {
  for (int i = 1; i < myargc; i++) {
    if (!stricmp(check, myargv[i])) {
      return true;
    }
  }
  return false;
}


#if defined __WATCOMC__
#define InitTimer()
#define waitUntilNextTickBoundary()
#define advanceTickSchedule()
#else
#define TICKS_PER_SECOND 60
#define TICK_INTERVAL_TICKS (UCLOCKS_PER_SEC / TICKS_PER_SECOND)


static bool noTimer;
static uclock_t nextTickTime;


static void InitTimer(void) {
  noTimer = M_CheckParm("-notimer");
  if (noTimer)
    return;

  nextTickTime = uclock();
}


static void waitUntilNextTickBoundary(void) {
  if (noTimer)
    return;

  for (;;) {
    uclock_t now = uclock();
    if (now >= nextTickTime) {
      break;
    }
    uclock_t remaining = nextTickTime - now;
    uclock_t remainingNs = (remaining * 1000000000ULL) / UCLOCKS_PER_SEC;
    if (remainingNs > 2000000ULL) {
      delay(1);
    }
  }
}


static void advanceTickSchedule(void) {
  if (noTimer)
    return;

  uclock_t now = uclock();
  nextTickTime += TICK_INTERVAL_TICKS;
  if (now > nextTickTime + TICK_INTERVAL_TICKS) {
    nextTickTime = now + TICK_INTERVAL_TICKS;
  }
}
#endif


#define KEYBOARDINT 9
#define KBDQUEUESIZE 32
static uint8_t keyboardqueue[KBDQUEUESIZE];
static int kbdtail, kbdhead;
static bool isKeyboardIsrSet = false;

#if defined __DJGPP__
static _go32_dpmi_seginfo oldkeyboardisr, newkeyboardisr;
#else
static void __interrupt __far (*oldkeyboardisr)(void);
#endif


static void __interrupt __far I_KeyboardISR(void)	
{
  // Get the scan code
  keyboardqueue[kbdhead & (KBDQUEUESIZE - 1)] = inp(0x60);
  kbdhead++;

  // acknowledge the interrupt
  outp(0x20, 0x20);
}


static void SDL_Init(void) {
  replaceInterrupt(oldkeyboardisr, newkeyboardisr, KEYBOARDINT, I_KeyboardISR);
  isKeyboardIsrSet = true;
}


#define SC_INDEX                0x3c4
#define SC_RESET                0
#define SC_MAPMASK              2
#define SC_MEMMODE              4

#define CRTC_INDEX              0x3d4
#define CRTC_V_TOTAL            0x06
#define CRTC_OVERFLOW           0x07
#define CRTC_MAXSCANLINE        0x09
#define CRTC_V_RETRACE          0x10
#define CRTC_V_ENDRETRACE       0x11
#define CRTC_V_DISPEND          0x12
#define CRTC_UNDERLINE          0x14
#define CRTC_V_BLANK            0x15
#define CRTC_V_ENDBLANK         0x16
#define CRTC_MODE               0x17

#define MISC_OUTPUT             0x3c2


typedef enum
{
  LFB,
  NOLFB,
  MODEX,
  MODE13H
} videocardsenum_t;


static videocardsenum_t videocard;
static bool isGraphicsModeSet = false;
static uint8_t *videomemory;


static void SDL_CreateWindow(void) {
  if (M_CheckParm("-mode13h")) {
    videocard = MODE13H;
  } else if (M_CheckParm("-modex")){
    videocard = MODEX;
  } else if (M_CheckParm("-nolfb")) {
    videocard = NOLFB;
  } else {
    videocard = LFB;
  }

  if (videocard == LFB) {
    videomemory = VBEinit640x480x8();
    if (videomemory == nullptr) {
      I_Error("Linear frame buffer not supported. Try command line argument -nolfb");
    }
  } else if (videocard == NOLFB) {
    union REGS r;
    r.w.ax = 0x4F02;
    r.w.bx = 0x101;
    int386(0x10, &r, &r);
    if (r.w.ax != 0x004F) {
        I_Error("VESA not supported. Try command line argument -modex or -mode13h");
    }

    __djgpp_nearptr_enable();
    videomemory = (uint8_t*)0xA0000 + __djgpp_conventional_base;
  } else {
    union REGS r;
    r.w.ax = 0x0013;
    int386(0x10, &r, &r);
    __djgpp_nearptr_enable();
    videomemory = (uint8_t*)0xA0000 + __djgpp_conventional_base;

    if (videocard == MODEX) {
      // This code is based on Michael Abrash's Graphics Programming Black Book, Special Edition
      // Chapter 47 -- Mode X: 256-Color VGA Magic
      // https://github.com/jagregory/abrash-black-book/blob/master/src/chapter-47.md

      // disable chain4 mode
      outp(SC_INDEX, SC_MEMMODE);
      outp(SC_INDEX + 1, 6);

      // synchronous reset while setting Misc Output
      //  for safety, even though clock unchanged
      outp(SC_INDEX, SC_RESET);
      outp(SC_INDEX + 1, 1);

      // select 25 MHz dot clock & 60 Hz scanning rate
      outp(MISC_OUTPUT, 0xe3);

      // undo reset (restart sequencer)
      outp(SC_INDEX, SC_RESET);
      outp(SC_INDEX + 1, 3);

      // remove write protect on various CRTC registers
      outp(CRTC_INDEX, CRTC_V_ENDRETRACE);
      outp(CRTC_INDEX + 1, inp(CRTC_INDEX + 1) & 0x7f);

      outp(CRTC_INDEX, CRTC_V_TOTAL);      outp(CRTC_INDEX + 1, 0x0d); // vertical total
      outp(CRTC_INDEX, CRTC_OVERFLOW);     outp(CRTC_INDEX + 1, 0x3e); // overflow (bit 8 of vertical counts)
      outp(CRTC_INDEX, CRTC_MAXSCANLINE);  outp(CRTC_INDEX + 1, 0x41); // cell height (2 to double-scan)
      outp(CRTC_INDEX, CRTC_V_RETRACE);    outp(CRTC_INDEX + 1, 0xea); // v sync start
      outp(CRTC_INDEX, CRTC_V_ENDRETRACE); outp(CRTC_INDEX + 1, 0xac); // v sync end and protect cr0-cr7
      outp(CRTC_INDEX, CRTC_V_DISPEND);    outp(CRTC_INDEX + 1, 0xdf); // vertical displayed
      outp(CRTC_INDEX, CRTC_UNDERLINE);    outp(CRTC_INDEX + 1, 0x00); // turn off dword mode
      outp(CRTC_INDEX, CRTC_V_BLANK);      outp(CRTC_INDEX + 1, 0xe7); // v blank start
      outp(CRTC_INDEX, CRTC_V_ENDBLANK);   outp(CRTC_INDEX + 1, 0x06); // v blank end
      outp(CRTC_INDEX, CRTC_MODE);         outp(CRTC_INDEX + 1, 0xe3); // turn on byte mode

      // enable writes to all four planes
      outp(SC_INDEX, SC_MAPMASK);
      outp(SC_INDEX + 1, 0x0f);

      memset(videomemory, 0, 0xffff);
    }
  }

  isGraphicsModeSet = true;
}


static void SDL_DestroyWindow(void) {
  if (videocard == LFB) {
    VBEshutdown();
  }

  union REGS r;
  r.w.ax = 0x0003;
  int386(0x10, &r, &r);
}


void presentFrame() {
  if (videocard == LFB) {
    memcpy(videomemory, pixelBuffer, screenWidth * screenHeight);
  } else if (videocard == NOLFB) {
    int bank_size           = 65536;
    int bank_number         = 0;
    int bytes_to_copy_count = screenWidth * screenHeight;
    uint8_t *src            = pixelBuffer;

    while (bytes_to_copy_count > 0) {
      int copy_size;
      union REGS r;
      r.w.ax = 0x4F05;
      r.w.bx = 0;
      r.w.dx = bank_number;
      int386(0x10, &r, &r);

      copy_size = bytes_to_copy_count > bank_size ? bank_size : bytes_to_copy_count;

      memcpy(videomemory, src, copy_size);

      bytes_to_copy_count -= copy_size;
      src                 += copy_size;
      bank_number++;
    }
  } else if (videocard == MODEX) {
    for (int p = 0; p < 4; p++) {
      outp(SC_INDEX + 1, 1 << p);
      uint8_t *src = pixelBuffer + p * 2;
      volatile uint8_t *dst = videomemory;
      for (int y = 0; y < 240; y++) {
        for (int x = 0; x < 320 / 4; x++) {
          *dst++ = *src;
          src += 8;
        }
        src += screenWidth;
      }
    }
  } else { // MODE13H
    uint8_t *src = pixelBuffer;
    uint8_t *dst = videomemory;
    uint8_t overflow = 0;
    for (int y = 0; y < 200; y++) {
      for (int x = 0; x < 320; x++) {
        *dst++ = *src;
        src += 2;
      }
      src += screenWidth;
      uint8_t newOverflow = overflow + 0x66;
      if (newOverflow < overflow) {
        src += screenWidth;
      }
      overflow = newOverflow;
    }
  }
}


static void initSDL(void) {
  SDL_Init();

  SDL_CreateWindow();

  pixelBuffer = new uint8_t[screenWidth * screenHeight];
  memset(pixelBuffer, 0, screenWidth * screenHeight);

  moviePlayer = new MoviePlayer();
}


void shutdownSDL() {
  if (moviePlayer) {
    moviePlayer->stop(false);
    delete moviePlayer;
    moviePlayer = nullptr;
  }

  delete[] pixelBuffer;
  pixelBuffer = nullptr;

  if (isGraphicsModeSet) {
    SDL_DestroyWindow();
  }

  if (isKeyboardIsrSet) {
    restoreInterrupt(KEYBOARDINT, oldkeyboardisr, newkeyboardisr);
  }
}


void syncMouse() {
  static int prevLeft = 0;
  int mx = 0;
  int my = 0;
  uint32_t buttons = 0;
  g_MouseXCurrent = mx;
  g_MouseYCurrent = my;
  int left = 0;
  g_MouseFlg = left;
  g_MouseActualFlg = left;
  if (left != 0 && prevLeft == 0) {
    g_MouseXDown = mx;
    g_MouseYDown = my;
  }
  prevLeft = left;
}

}  // namespace


_Noreturn void I_Error(const char *error, ...) {
  va_list argptr;

  static bool firstTime = true;
  if (firstTime) {
    firstTime = false;
    shutdownAudio();
    shutdownSDL();
  }

  va_start(argptr, error);
  vprintf(error, argptr);
  va_end(argptr);
  printf("\n");
  exit(1);
}


typedef enum EventType {
  EVENT_KEY_DOWN,
  EVENT_KEY_UP,
  EVENT_MOUSE_BUTTON_DOWN
} EventType;

typedef struct Event {
  EventType type;
  uint8_t scancode;
} Event;


#define SC_ESCAPE     0x01
#define SC_E          0x12
#define SC_P          0x19
#define SC_M          0x32
#define SC_LSHIFT     0x2a
#define SC_RSHIFT     0x36
#define SC_SPACE      0x39
#define SC_F10        0x44
#define SC_UPARROW    0x48
#define SC_DOWNARROW  0x50
#define SC_LEFTARROW  0x4b
#define SC_RIGHTARROW 0x4d
#define SC_PAUSE      0xff


static bool SDL_PollEvent(Event *event) {
  while (kbdtail < kbdhead)	{
    uint8_t k = keyboardqueue[kbdtail & (KBDQUEUESIZE - 1)];
    kbdtail++;

    // extended keyboard shift key bullshit
    if ((k & 0x7f) == SC_LSHIFT || (k & 0x7f) == SC_RSHIFT) {
      if (keyboardqueue[(kbdtail - 2) & (KBDQUEUESIZE - 1)] == 0xe0) {
        continue;
      }
      k &= 0x80;
      k |= SC_RSHIFT;
    }

    if (k == 0xe0) {
      continue;               // special / pause keys
    }
    if (keyboardqueue[(kbdtail - 2) & (KBDQUEUESIZE - 1)] == 0xe1) {
      continue;                               // pause key bullshit
    }

    if (k == 0xc5 && keyboardqueue[(kbdtail - 2) & (KBDQUEUESIZE - 1)] == 0x9d) {
      event->type     = EVENT_KEY_DOWN;
      event->scancode = SC_PAUSE;
      return true;
    }

    if (k & 0x80) {
      event->type = EVENT_KEY_UP;
    } else {
      event->type = EVENT_KEY_DOWN;
    }

    event->scancode = k & 0x7f;
    return true;
  }

  return false;
}


int main(int argc, char **argv) {
  myargc = argc;
  myargv = argv;

  initSDL();

  if (!initAudio()) {
    shutdownAudio();
    shutdownSDL();
    return 1;
  }

  initMoonChild(pixelBuffer, screenWidth, screenHeight, moviePlayer);

  if (M_CheckParm("-timedemo")) {
    extern HEARTBEAT_FN MC_startdemo(void);
    extern UINT16 ingameflg;
    static int frames;
    heartbeat = (HEARTBEAT_FN)MC_startdemo;
    clock_t starttime = clock();
    do {
      gameTick(pixelBuffer, screenWidth, screenHeight, screenWidth, nullptr);
      presentFrame();
      frames++;
    } while (ingameflg);
    clock_t endtime = clock();
    int seconds = (endtime - starttime) * 1000 / CLOCKS_PER_SEC;
    int fps = frames * 1000 * CLOCKS_PER_SEC / (endtime - starttime);
    I_Error("%i frames in %i.%.3i seconds = %i.%.3i frames per second",
            frames, seconds / 1000, seconds % 1000, fps / 1000, fps % 1000);
  }

  InitTimer();

  bool running = true;
  while (running) {
    Event e;
    while (SDL_PollEvent(&e)) {
      if (e.scancode == SC_F10) {
        running = false;
      }
      if (e.type == EVENT_KEY_DOWN) {
        if (moviePlayer && moviePlayer->isPlaying()) {
          moviePlayer->stop(false);
          continue;
        }
        switch (e.scancode) {
          case SC_UPARROW:  // move up
            framework_EventHandle(FW_KEYDOWN,(int) prefs->upkey);
            break;
          case SC_DOWNARROW:  // move down
            framework_EventHandle(FW_KEYDOWN,(int) prefs->downkey);
            break;
          case SC_LEFTARROW:  // move left
            framework_EventHandle(FW_KEYDOWN,(int) prefs->leftkey);
            break;
          case SC_RIGHTARROW:  // move right
            framework_EventHandle(FW_KEYDOWN,(int) prefs->rightkey);
            break;
          case SC_SPACE:   // fire or switch
            framework_EventHandle(FW_KEYDOWN,(int) prefs->shootkey);
            break;
          case SC_ESCAPE:  // break out of level
            framework_EventHandle(FW_KEYDOWN,(int) 'Q');
            break;
          case SC_E:  // If editor is compiled (define in mc.cpp) then this is the key to show it
            framework_EventHandle(FW_KEYDOWN,(int) 'E');
            break;
          case SC_P:  // if editor is compiled (define in mc.cpp) then this is the key to show patterns(tiles)
            framework_EventHandle(FW_KEYDOWN,(int) 'P');
            break;
          case SC_M: {
            char introMoviePath[] = "assets/movies/intro.mp4";
            bool movieStarted = moviePlayer->playFile(introMoviePath, onMovieDone, nullptr);
            if (!movieStarted) {
              printf("movie: failed to play %s\n", introMoviePath);
            } else {
              printf("movie: playing %s\n", introMoviePath);
            }
            break;
          }
          default:
            break;
        }
      }
      if (moviePlayer && moviePlayer->isPlaying()) {
        if (e.type == EVENT_MOUSE_BUTTON_DOWN) {
          moviePlayer->stop(false);
          continue;
        }
      }
      if (e.type == EVENT_KEY_UP) {
        switch (e.scancode) {
          case SC_UPARROW:  // move up
            framework_EventHandle(FW_KEYUP,(int) prefs->upkey);
            break;
          case SC_DOWNARROW:  // move down
            framework_EventHandle(FW_KEYUP,(int) prefs->downkey);
            break;
          case SC_LEFTARROW:  // move left
            framework_EventHandle(FW_KEYUP,(int) prefs->leftkey);
            break;
          case SC_RIGHTARROW:  // move right
            framework_EventHandle(FW_KEYUP,(int) prefs->rightkey);
            break;
          case SC_SPACE:   // fire or switch
            framework_EventHandle(FW_KEYUP,(int) prefs->shootkey);
            break;
          case SC_ESCAPE:  // break out of level
            framework_EventHandle(FW_KEYUP,(int) 'Q');
            break;
          case SC_E:  // If editor is compiled (define in mc.cpp) then this is the key to show it
            framework_EventHandle(FW_KEYUP,(int) 'E');
            break;
          case SC_P:  // if editor is compiled (define in mc.cpp) then this is the key to show patterns(tiles)
            framework_EventHandle(FW_KEYUP,(int) 'P');
            break;
          default:
            break;
        }
      }
    }

    syncMouse();

    waitUntilNextTickBoundary();

    if (moviePlayer && moviePlayer->isPlaying()) {
      moviePlayer->update(pixelBuffer, screenWidth, screenHeight, screenWidth);
    } else {
      gameTick(pixelBuffer, screenWidth, screenHeight, screenWidth, nullptr);
    }

    if (movieDoneSignal) {
      movieDoneSignal = false;
      if (movieFinishedNaturally) {
        printf("movie: playback finished\n");
      } else {
        printf("movie: playback stopped by input\n");
      }
    }

    presentFrame();

    advanceTickSchedule();
  }

  shutdownAudio();
  shutdownSDL();
  return 0;
}
