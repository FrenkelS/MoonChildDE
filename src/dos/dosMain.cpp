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

#define _IN_MAIN
#include "frm_int.hpp"

#include "moonchild/globals.hpp"
#include "moonchild/mc.hpp"
#include "moonchild/prefs.hpp"


//#define DEBUG_GRAPHICS
#define DEBUG_TIMER


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


namespace {

const int screenWidth = 640;
const int screenHeight = 480;
const int bytesPerPixel = 1;


MoviePlayer *moviePlayer = nullptr;

uint8_t *pixelBuffer = nullptr;
int pixelBufferPitch = 0;

bool movieFinishedNaturally = false;
bool movieDoneSignal = false;

void onMovieDone(bool naturalEnd, void *userData) {
  (void)userData;
  movieFinishedNaturally = naturalEnd;
  movieDoneSignal = true;
}


#if defined DEBUG_TIMER || defined __WATCOMC__
#define InitTimer()
#define waitUntilNextTickBoundary()
#define advanceTickSchedule()
#else
#define TICKS_PER_SECOND 60
#define TICK_INTERVAL_TICKS (UCLOCKS_PER_SEC / TICKS_PER_SECOND)


static uclock_t nextTickTime = 0;


static void InitTimer(void) {
  nextTickTime = uclock();
}


static void waitUntilNextTickBoundary(void) {
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


static bool isGraphicsModeSet = false;
static uint8_t *videomemory;


static void SDL_CreateWindow(void) {
  union REGS r;

#if defined DEBUG_GRAPHICS
  r.w.ax = 0x0013;
#else
  r.w.ax = 0x4F02;
  r.w.bx = 0x101;
#endif
  int386(0x10, &r, &r);

  __djgpp_nearptr_enable();
  videomemory = (uint8_t*)0xA0000 + __djgpp_conventional_base;

  isGraphicsModeSet = true;
}


static void initSDL(void) {
  SDL_Init();

  InitTimer();

  SDL_CreateWindow();

  pixelBufferPitch = screenWidth * bytesPerPixel;
  pixelBuffer = new uint8_t[pixelBufferPitch * screenHeight];
  memset(pixelBuffer, 0, pixelBufferPitch * screenHeight);

  moviePlayer = new MoviePlayer();
}


static void SDL_DestroyWindow(void) {
  union REGS r;
  r.w.ax = 0x0003;
  int386(0x10, &r, &r);
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


void presentFrame() {
#if defined DEBUG_GRAPHICS
  uint8_t *src = pixelBuffer;
  uint8_t *dst = videomemory;
  int x, y;
  uint8_t overflow = 0;
  for (y = 0; y < 200; y++) {
    for (x = 0; x < 320; x++) {
      *dst++ = *src;
      src += 2 * bytesPerPixel;
    }
    src += screenWidth * bytesPerPixel;
    uint8_t newOverflow = overflow + 0x66;
    if (newOverflow < overflow) {
      src += screenWidth * bytesPerPixel;
    }
    overflow = newOverflow;
  }
#else
  int bank_size = 65536;
  int bank_number = 0;
  int todo = screenWidth * screenHeight * bytesPerPixel;
  uint8_t *memory_buffer = pixelBuffer;

  while (todo > 0) {
    int copy_size;
    union REGS r;
    r.w.ax = 0x4F05;
    r.w.bx = 0;
    r.w.dx = bank_number;
    int386(0x10, &r, &r);

    copy_size = todo > bank_size ? bank_size : todo;

    memcpy(videomemory, memory_buffer, copy_size);

    todo          -= copy_size;
    memory_buffer += copy_size;
    bank_number++;
  }
#endif
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
  (void)argc;
  (void)argv;

  initSDL();

  if (!initAudio()) {
    shutdownAudio();
    shutdownSDL();
    return 1;
  }

  initMoonChild(pixelBuffer, screenWidth, screenHeight, moviePlayer);

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
      moviePlayer->update(pixelBuffer, screenWidth, screenHeight, pixelBufferPitch);
    } else {
      gameTick(pixelBuffer, screenWidth, screenHeight, pixelBufferPitch, nullptr);
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
