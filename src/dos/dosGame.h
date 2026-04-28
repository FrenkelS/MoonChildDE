#pragma once

#include <stdint.h>
class MoviePlayer;


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


void gameTick(uint8_t *pixels, int width, int height, int pitch, uint8_t *keyboardState);
void keyUp(int key);
void keyDown(int key);
void initMoonChild(unsigned char *pixelBuffer, int width, int height, MoviePlayer *moviePlayer);
void resetProgress();
void enableCheat();