#include <dos.h>
#include <dpmi.h>
#include <go32.h>
#include <SDL.h>
#include <time.h>
#include <sys/nearptr.h>


//#define DEBUG


//**************************************************************************************
//
// Input code
//

#define KEYBOARDINT 9
#define KBDQUEUESIZE 32
static uint8_t keyboardqueue[KBDQUEUESIZE];
static int kbdtail, kbdhead;
static bool isKeyboardIsrSet = false;

static _go32_dpmi_seginfo oldkeyboardisr, newkeyboardisr;


static void I_KeyboardISR(void)	
{
  // Get the scan code
  keyboardqueue[kbdhead & (KBDQUEUESIZE - 1)] = inportb(0x60);
  kbdhead++;

  // acknowledge the interrupt
  outportb(0x20, 0x20);
}


bool SDL_Init(SDL_InitFlags) {
  _go32_dpmi_get_protected_mode_interrupt_vector(KEYBOARDINT, &oldkeyboardisr);

  newkeyboardisr.pm_selector = _go32_my_cs();
  newkeyboardisr.pm_offset   = (uint32_t)I_KeyboardISR;
  _go32_dpmi_allocate_iret_wrapper(&newkeyboardisr);
  _go32_dpmi_set_protected_mode_interrupt_vector(KEYBOARDINT, &newkeyboardisr);

  isKeyboardIsrSet = true;

  return true;
}


void SDL_Quit(void) {
  if (isKeyboardIsrSet) {
    _go32_dpmi_set_protected_mode_interrupt_vector(KEYBOARDINT, &oldkeyboardisr);
    _go32_dpmi_free_iret_wrapper(&newkeyboardisr);
  }
}


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


bool SDL_PollEvent(SDL_Event *event) {
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


Uint32 SDL_GetMouseState(float*, float*) {
  return 0;
}


const bool *SDL_GetKeyboardState(int*) {
  return NULL;
}


//**************************************************************************************
//
// Timer code
//

#if defined DEBUG
Uint64 SDL_GetPerformanceFrequency(void) {
  return 60;
}

Uint64 SDL_GetPerformanceCounter(void) {
  static Uint64 i = 0;
  return i++;
}

void SDL_Delay(Uint32 ms) {
}
#else
Uint64 SDL_GetPerformanceFrequency(void) {
  return UCLOCKS_PER_SEC;
}

Uint64 SDL_GetPerformanceCounter(void) {
  return uclock();
}

void SDL_Delay(Uint32 ms) {
  delay(ms);
}
#endif


//**************************************************************************************
//
// Video code
//

struct SDL_Window {
  char dummy;
};


SDL_Window *SDL_CreateWindow(const char*, int w, int h, SDL_WindowFlags) {
  __dpmi_regs r;

#if defined DEBUG
  __djgpp_nearptr_enable();
  r.x.ax = 0x0013;
#else
  r.x.ax = 0x4F02;
  r.x.bx = 0x112;
#endif
  __dpmi_int(0x10, &r);

  static SDL_Window window;  
  return &window;
}


void SDL_DestroyWindow(SDL_Window*) {
  __dpmi_regs r;
  r.x.ax = 0x0003;
  __dpmi_int(0x10, &r);
}


bool SDL_UpdateTexture(SDL_Texture*, const SDL_Rect*, const void *pixels, int pitch) {
#if defined DEBUG
  uint8_t *src = (uint8_t*)pixels;
  uint8_t *dst = (uint8_t*)0xA0000 + __djgpp_conventional_base;
  for (int y = 0; y < 200; y++) {
    for (int x = 0; x < 320; x++) {
      uint8_t r = *src;
      *dst++ = 16 + r / 16;
      src += 8;
    }
    src += 640 * 4;
  }
#else
  int bank_size = 65536;
  int bank_number = 0;
  int todo = 640 * 480 * 4;
  uint8_t *memory_buffer = (uint8_t*)pixels;

  while (todo > 0) {
    __dpmi_regs r;
    r.x.ax = 0x4F05;
    r.x.bx = 0;
    r.x.dx = bank_number;
    __dpmi_int(0x10, &r);

    int copy_size = todo > bank_size ? bank_size : todo;

    dosmemput(memory_buffer, copy_size, 0xA0000);

    todo          -= copy_size;
    memory_buffer += copy_size;
    bank_number++;
  }
#endif

  return false;
}


struct SDL_Renderer {
  char dummy;
};


SDL_Renderer *SDL_CreateRenderer(SDL_Window*, const char*) {
  static SDL_Renderer renderer;
  return &renderer;
}


void SDL_DestroyRenderer(SDL_Renderer*) {
}


SDL_Texture *SDL_CreateTexture(SDL_Renderer*, SDL_PixelFormat, SDL_TextureAccess, int, int) {
  static SDL_Texture texture;
  return &texture;
}


void SDL_DestroyTexture(SDL_Texture*) {
}


bool SDL_SetTextureBlendMode(SDL_Texture*, SDL_BlendMode) {
  return false;
}


bool SDL_RenderClear(SDL_Renderer*) {
  return false;
}


bool SDL_RenderTexture(SDL_Renderer*, SDL_Texture*, const SDL_FRect*, const SDL_FRect*) {
  return false;
}


bool SDL_RenderPresent(SDL_Renderer*) {
  return false;
}


//**************************************************************************************
//
// IO code
//

char *SDL_GetPrefPath(const char*, const char*) {
  return NULL;
}


const char *SDL_GetBasePath(void) {
  return NULL;
}


void SDL_free(void*) {
}


//**************************************************************************************
//
// Misc code
//

const char *SDL_GetError(void) {
  return "SDL_GetError() is not implemented";
}
