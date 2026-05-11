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


typedef enum
{
  LFB,          // 640x480
  NOLFB,        // 640x480
  MODEX320X480, // 320x480
  MODEX,        // 320x240
  MODE13H       // 320x200
} videocardsenum_t;


static videocardsenum_t videocard;
static bool isGraphicsModeSet = false;
static uint8_t *videomemory;


//
// registers & subregisters
//
#define SC_INDEX        0x3C4
#define SC_DATA         0x3C5
#define SYNC_RESET      0
#define MAP_MASK        2
#define MEMORY_MODE     4

#define GC_INDEX        0x3CE
#define GC_DATA         0x3CF
#define GRAPHICS_MODE   5
#define MISCELLANOUS    6

#define CRTC_INDEX      0x3D4
#define CRTC_DATA       0x3D5
#define MAX_SCAN_LINE   9
#define UNDERLINE       0x14
#define	MODE_CONTROL    0x17

//
// register-set commands
//
#define VRS_END         0
#define VRS_BYTE_OUT    1
#define VRS_BYTE_RMW    2
#define VRS_WORD_OUT    3


static const int vrs320x240x256planar[] = {
//
// switch to linear, non-chain4 mode
//
  VRS_BYTE_OUT, SC_INDEX, SYNC_RESET,
  VRS_BYTE_OUT, SC_DATA,  1,

  VRS_BYTE_OUT, SC_INDEX, MEMORY_MODE,
  VRS_BYTE_RMW, SC_DATA, ~0x08, 0x04,
  VRS_BYTE_OUT, GC_INDEX, GRAPHICS_MODE,
  VRS_BYTE_RMW, GC_DATA, ~0x13, 0x00,
  VRS_BYTE_OUT, GC_INDEX, MISCELLANOUS,
  VRS_BYTE_RMW, GC_DATA, ~0x02, 0x00,

  VRS_BYTE_OUT, SC_INDEX, SYNC_RESET,
  VRS_BYTE_OUT, SC_DATA,  3,

//
// unprotect CRTC0 through CRTC0
//
  VRS_BYTE_OUT, CRTC_INDEX, 0x11,
  VRS_BYTE_RMW, CRTC_DATA, ~0x80, 0x00,

//
// set up the CRT Controller
//
  VRS_WORD_OUT, CRTC_INDEX, 0x0D06,
  VRS_WORD_OUT, CRTC_INDEX, 0x3E07,
  VRS_WORD_OUT, CRTC_INDEX, 0x4109,
  VRS_WORD_OUT, CRTC_INDEX, 0xEA10,
  VRS_WORD_OUT, CRTC_INDEX, 0xAC11,
  VRS_WORD_OUT, CRTC_INDEX, 0xDF12,
  VRS_WORD_OUT, CRTC_INDEX, 0x0014,
  VRS_WORD_OUT, CRTC_INDEX, 0xE715,
  VRS_WORD_OUT, CRTC_INDEX, 0x0616,
  VRS_WORD_OUT, CRTC_INDEX, 0xE317,

  VRS_END,
};


static const int vrs320x480x256planar[] = {
//
// switch to linear, non-chain4 mode
//
  VRS_BYTE_OUT, SC_INDEX, SYNC_RESET,
  VRS_BYTE_OUT, SC_DATA,  1,

  VRS_BYTE_OUT, SC_INDEX, MEMORY_MODE,
  VRS_BYTE_RMW, SC_DATA, ~0x08, 0x04,
  VRS_BYTE_OUT, GC_INDEX, GRAPHICS_MODE,
  VRS_BYTE_RMW, GC_DATA, ~0x10, 0x00,
  VRS_BYTE_OUT, GC_INDEX, MISCELLANOUS,
  VRS_BYTE_RMW, GC_DATA, ~0x02, 0x00,

  VRS_BYTE_OUT, SC_INDEX, SYNC_RESET,
  VRS_BYTE_OUT, SC_DATA,  3,

//
// unprotect CRTC0 through CRTC0
//
  VRS_BYTE_OUT, CRTC_INDEX, 0x11,
  VRS_BYTE_RMW, CRTC_DATA, ~0x80, 0x00,

//
// stop scanning each line twice
//
  VRS_BYTE_OUT, CRTC_INDEX, MAX_SCAN_LINE,
  VRS_BYTE_RMW, CRTC_DATA, ~0x1F, 0x00,

//
// change the CRTC from doubleword to byte mode
//
  VRS_BYTE_OUT, CRTC_INDEX, UNDERLINE,
  VRS_BYTE_RMW, CRTC_DATA, ~0x40, 0x00,
  VRS_BYTE_OUT, CRTC_INDEX, MODE_CONTROL,
  VRS_BYTE_RMW, CRTC_DATA, ~0x00, 0x40,

//
// set up the CRT Controller
//
  VRS_WORD_OUT, CRTC_INDEX, 0x0D06,
  VRS_WORD_OUT, CRTC_INDEX, 0x3E07,
  VRS_WORD_OUT, CRTC_INDEX, 0xEA10,
  VRS_WORD_OUT, CRTC_INDEX, 0xAC11,
  VRS_WORD_OUT, CRTC_INDEX, 0xDF12,
  VRS_WORD_OUT, CRTC_INDEX, 0xE715,
  VRS_WORD_OUT, CRTC_INDEX, 0x0616,

  VRS_END,
};


static void VideoRegisterSet(const int *pregset)
{
  int port, temp0, temp1, temp2;

  for (;;) {
    switch (*pregset++) {
      case VRS_BYTE_OUT:
        port = *pregset++;
        outp(port, *pregset++);
        break;

      case VRS_BYTE_RMW:
        port = *pregset++;
        temp0 = *pregset++;
        temp1 = *pregset++;
        temp2 = inp(port);
        temp2 &= temp0;
        temp2 |= temp1;
        outp(port, temp2);
        break;

      case VRS_WORD_OUT:
        port = *pregset++;
        outp(port,     *pregset & 0xFF);
        outp(port + 1, *pregset >> 8);
        pregset++;
        break;

      case VRS_END:
        return;
    }
  }
}


static void SDL_CreateWindow(void) {
  videocard = LFB;
  if (M_CheckParm("-nolfb")) {
    videocard = NOLFB;
  } else if (M_CheckParm("-modex320x480")) {
    videocard = MODEX320X480;
  } else if (M_CheckParm("-modex")){
    videocard = MODEX;
  } else if (M_CheckParm("-mode13h")) {
    videocard = MODE13H;
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
      VideoRegisterSet(vrs320x240x256planar);
      outp(SC_INDEX, MAP_MASK);
    } else if (videocard == MODEX320X480) {
      VideoRegisterSet(vrs320x480x256planar);
      outp(SC_INDEX, MAP_MASK);
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
  } else if (videocard == MODEX320X480) {
    for (int p = 0; p < 4; p++) {
      outp(SC_DATA, 1 << p);
      uint8_t *src = pixelBuffer + p * 2;
      volatile uint8_t *dst = videomemory;
      for (int y = 0; y < 480; y++) {
        for (int x = 0; x < 320 / 4; x++) {
          *dst++ = *src;
          src += 8;
        }
      }
    }
  } else if (videocard == MODEX) {
    for (int p = 0; p < 4; p++) {
      outp(SC_DATA, 1 << p);
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
