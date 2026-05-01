#include <conio.h>
#include <cstdint>
#include <cstdio>
#include <cstring>
#include <dos.h>
#include <string.h>
#include <time.h>

#include "Audio.h"
#include "dosGame.h"
#include "MoviePlayer.h"
#include "Util.h"

#define _IN_MAIN
#include "frm_int.hpp"


//#define DEBUG_GRAPHICS
//#define DEBUG_TIMER


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
static uint64_t SDL_GetPerformanceFrequency(void) {
  return 60;
}


static uint64_t SDL_GetPerformanceCounter(void) {
  static uint64_t i = 0;
  return i++;
}


static void SDL_Delay(uint32_t ms) {
}
#else
static uint64_t SDL_GetPerformanceFrequency(void) {
  return UCLOCKS_PER_SEC;
}


static uint64_t SDL_GetPerformanceCounter(void) {
  return uclock();
}


static void SDL_Delay(uint32_t ms) {
  delay(ms);
}
#endif


static uint64_t performanceFrequency = 0;
static uint64_t tickIntervalTicks = 0;
static uint64_t nextTickTime = 0;
static const int ticksPerSecond = 60;


static void waitUntilNextTickBoundary(void) {
  for (;;) {
    uint64_t now = SDL_GetPerformanceCounter();
    if (now >= nextTickTime) {
      break;
    }
    uint64_t remaining = nextTickTime - now;
    uint64_t remainingNs = (remaining * 1000000000ULL) / performanceFrequency;
    if (remainingNs > 2000000ULL) {
      SDL_Delay(1);
    }
  }
}


static void advanceTickSchedule(void) {
  uint64_t now = SDL_GetPerformanceCounter();
  nextTickTime += tickIntervalTicks;
  if (now > nextTickTime + tickIntervalTicks) {
    nextTickTime = now + tickIntervalTicks;
  }
}


static void InitTimer(void) {
  performanceFrequency = SDL_GetPerformanceFrequency();
  tickIntervalTicks = performanceFrequency / (uint64_t)ticksPerSecond;
  nextTickTime = SDL_GetPerformanceCounter();
}


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
  uint32_t overflow = 0;
  for (y = 0; y < 200; y++) {
    for (x = 0; x < 320; x++) {
      *dst++ = *src;
      src += 2 * bytesPerPixel;
    }
    src += screenWidth * bytesPerPixel;
    uint32_t newOverflow = overflow + 0x66000000;
    if ((int32_t)newOverflow < (int32_t)overflow) {
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


typedef enum SDL_EventType {
  SDL_EVENT_QUIT              = 0x100,
  SDL_EVENT_KEY_DOWN          = 0x300,
  SDL_EVENT_KEY_UP            = 0x301,
  SDL_EVENT_MOUSE_BUTTON_DOWN = 0x401,
  SDL_EVENT_FINGER_DOWN       = 0x700
} SDL_EventType;


typedef struct SDL_KeyboardEvent {
  SDL_Scancode scancode;
  bool repeat;
} SDL_KeyboardEvent;


typedef struct SDL_Event {
  uint32_t type;
  SDL_KeyboardEvent key;
} SDL_Event;


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


static bool SDL_PollEvent(SDL_Event *event) {
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
      event->type         = SDL_EVENT_KEY_DOWN;
      event->key.scancode = SDL_SCANCODE_PAUSE;
      event->key.repeat   = false;
      return true;
    }

    if (k & 0x80) {
      event->type = SDL_EVENT_KEY_UP;
    } else {
      event->type = SDL_EVENT_KEY_DOWN;
    }

    k &= 0x7f;
    if (k == SC_F10) {
      event->type = SDL_EVENT_QUIT;
    }

    switch (k) {
      case SC_UPARROW:    event->key.scancode = SDL_SCANCODE_UP;      break;
      case SC_DOWNARROW:  event->key.scancode = SDL_SCANCODE_DOWN;    break;
      case SC_LEFTARROW:  event->key.scancode = SDL_SCANCODE_LEFT;    break;
      case SC_RIGHTARROW: event->key.scancode = SDL_SCANCODE_RIGHT;   break;
      case SC_SPACE:      event->key.scancode = SDL_SCANCODE_SPACE;   break;
      case SC_ESCAPE:     event->key.scancode = SDL_SCANCODE_ESCAPE;  break;
      case SC_E:          event->key.scancode = SDL_SCANCODE_E;       break;
      case SC_P:          event->key.scancode = SDL_SCANCODE_P;       break;
      case SC_M:          event->key.scancode = SDL_SCANCODE_M;       break;
      default:            event->key.scancode = SDL_SCANCODE_UNKNOWN; break;
    }

    event->key.repeat = false;
    return true;
  }

  return false;
}


static const bool *SDL_GetKeyboardState(int *numkeys) {
  return NULL;
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
    SDL_Event e;
    while (SDL_PollEvent(&e)) {
      if (e.type == SDL_EVENT_QUIT) {
        running = false;
      }
      if (e.type == SDL_EVENT_KEY_DOWN && e.key.repeat == 0) {
        if (moviePlayer && moviePlayer->isPlaying()) {
          moviePlayer->stop(false);
          continue;
        }
        switch (e.key.scancode) {
          case SDL_SCANCODE_UP:
            keyDown(SDL_SCANCODE_UP);
            break;
          case SDL_SCANCODE_DOWN:
            keyDown(SDL_SCANCODE_DOWN);
            break;
          case SDL_SCANCODE_LEFT:
            keyDown(SDL_SCANCODE_LEFT);
            break;
          case SDL_SCANCODE_RIGHT:
            keyDown(SDL_SCANCODE_RIGHT);
            break;
          case SDL_SCANCODE_SPACE:
            keyDown(SDL_SCANCODE_SPACE);
            break;
          case SDL_SCANCODE_ESCAPE:
            keyDown(SDL_SCANCODE_ESCAPE);
            break;
          case SDL_SCANCODE_E:
            keyDown(SDL_SCANCODE_E);
            break;
          case SDL_SCANCODE_P:
            keyDown(SDL_SCANCODE_P);
            break;
          case SDL_SCANCODE_M: {
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
        if (e.type == SDL_EVENT_MOUSE_BUTTON_DOWN || e.type == SDL_EVENT_FINGER_DOWN) {
          moviePlayer->stop(false);
          continue;
        }
      }
      if (e.type == SDL_EVENT_KEY_UP && e.key.repeat == 0) {
        switch (e.key.scancode) {
          case SDL_SCANCODE_UP:
            keyUp(SDL_SCANCODE_UP);
            break;
          case SDL_SCANCODE_DOWN:
            keyUp(SDL_SCANCODE_DOWN);
            break;
          case SDL_SCANCODE_LEFT:
            keyUp(SDL_SCANCODE_LEFT);
            break;
          case SDL_SCANCODE_RIGHT:
            keyUp(SDL_SCANCODE_RIGHT);
            break;
          case SDL_SCANCODE_SPACE:
            keyUp(SDL_SCANCODE_SPACE);
            break;
          case SDL_SCANCODE_ESCAPE:
            keyUp(SDL_SCANCODE_ESCAPE);
            break;
          case SDL_SCANCODE_E:
            keyUp(SDL_SCANCODE_E);
            break;
          case SDL_SCANCODE_P:
            keyUp(SDL_SCANCODE_P);
            break;
          default:
            break;
        }
      }
    }

    syncMouse();

    waitUntilNextTickBoundary();

    int keyCount = 0;
    uint8_t *keyboardState = (uint8_t *)SDL_GetKeyboardState(&keyCount);
    (void)keyCount;

    if (moviePlayer && moviePlayer->isPlaying()) {
      moviePlayer->update(pixelBuffer, screenWidth, screenHeight, pixelBufferPitch);
    } else {
      gameTick(pixelBuffer, screenWidth, screenHeight, pixelBufferPitch, keyboardState);
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
