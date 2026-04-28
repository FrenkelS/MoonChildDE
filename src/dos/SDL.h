#ifndef __SDL__
#define __SDL__

#include <stdbool.h>
#include <stdint.h>


#ifdef __cplusplus
extern "C" {
#endif


typedef uint8_t Uint8;
typedef uint32_t Uint32;
typedef uint64_t Uint64;


//**************************************************************************************
//
// Input code
//

typedef enum SDL_EventType {
  SDL_EVENT_QUIT              = 0x100,
  SDL_EVENT_KEY_DOWN          = 0x300,
  SDL_EVENT_KEY_UP            = 0x301,
  SDL_EVENT_MOUSE_BUTTON_DOWN = 0x401,
  SDL_EVENT_FINGER_DOWN       = 0x700
} SDL_EventType;

typedef enum SDL_Scancode {
  SDL_SCANCODE_UNKNOWN =  0,
  SDL_SCANCODE_E       =  8,
  SDL_SCANCODE_M       = 16,
  SDL_SCANCODE_P       = 19,
  SDL_SCANCODE_ESCAPE  = 41,
  SDL_SCANCODE_SPACE   = 44,
  SDL_SCANCODE_F10     = 67,
  SDL_SCANCODE_PAUSE   = 72,
  SDL_SCANCODE_RIGHT   = 79,
  SDL_SCANCODE_LEFT    = 80,
  SDL_SCANCODE_DOWN    = 81,
  SDL_SCANCODE_UP      = 82
} SDL_Scancode;

typedef struct SDL_KeyboardEvent {
  SDL_Scancode scancode;
  bool repeat;
} SDL_KeyboardEvent;

typedef struct SDL_Event {
  Uint32 type;
  SDL_KeyboardEvent key;
} SDL_Event;

typedef Uint32 SDL_InitFlags;
#define SDL_INIT_AUDIO 0x00000010u
#define SDL_INIT_VIDEO 0x00000020u

#define SDL_BUTTON_LMASK 1u

bool SDL_Init(SDL_InitFlags);
void SDL_Quit(void);
bool SDL_PollEvent(SDL_Event*);
Uint32 SDL_GetMouseState(float*, float*);
const bool *SDL_GetKeyboardState(int*);


//**************************************************************************************
//
// Timer code
//

Uint64 SDL_GetPerformanceFrequency(void);
Uint64 SDL_GetPerformanceCounter(void);
void SDL_Delay(Uint32 ms);


//**************************************************************************************
//
// Video code
//

typedef Uint64 SDL_WindowFlags;

typedef struct SDL_Window   SDL_Window;
typedef struct SDL_Renderer SDL_Renderer;

typedef struct SDL_Texture {
  char dummy;
} SDL_Texture;

typedef struct SDL_Rect {
  char dummy;
} SDL_Rect;

typedef enum SDL_PixelFormat {
  SDL_PIXELFORMAT_BGRA32 = 0x16362004u
} SDL_PixelFormat;

typedef enum SDL_TextureAccess {
  SDL_TEXTUREACCESS_STREAMING = 1
} SDL_TextureAccess;

typedef Uint32 SDL_BlendMode;
#define SDL_BLENDMODE_NONE 0x00000000u

typedef struct SDL_FRect {
  char dummy;
} SDL_FRect;

SDL_Window *SDL_CreateWindow(const char*, int w, int h, SDL_WindowFlags);
void SDL_DestroyWindow(SDL_Window*);
bool SDL_UpdateTexture(SDL_Texture*, const SDL_Rect*, const void *pixel, int pitch);
SDL_Renderer *SDL_CreateRenderer(SDL_Window*, const char*);
void SDL_DestroyRenderer(SDL_Renderer*);
SDL_Texture *SDL_CreateTexture(SDL_Renderer*, SDL_PixelFormat, SDL_TextureAccess, int, int);
void SDL_DestroyTexture(SDL_Texture*);
bool SDL_SetTextureBlendMode(SDL_Texture*, SDL_BlendMode);
bool SDL_RenderClear(SDL_Renderer*);
bool SDL_RenderTexture(SDL_Renderer*, SDL_Texture*, const SDL_FRect*, const SDL_FRect*);
bool SDL_RenderPresent(SDL_Renderer*);


//**************************************************************************************
//
// IO code
//

char *SDL_GetPrefPath(const char*, const char*);
const char *SDL_GetBasePath(void);
void SDL_free(void*);


//**************************************************************************************
//
// Misc code
//

const char *SDL_GetError(void);


#ifdef __cplusplus
}
#endif


#endif
