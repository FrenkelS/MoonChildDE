#pragma once

#include <SDL.h>

#include <stdint.h>
class MoviePlayer;

void gameTick(uint8_t *pixels, int width, int height, int pitch, Uint8 *keyboardState);
void keyUp(int key);
void keyDown(int key);
void initMoonChild(unsigned char *pixelBuffer, int width, int height, MoviePlayer *moviePlayer);
void resetProgress();
void enableCheat();