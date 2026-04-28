#include "Audio.h"
#include "Game.h"
#include "MoviePlayer.h"
#include "Util.h"

#include <SDL.h>

#include <cstdint>
#include <cstdio>
#include <cstring>

#define _IN_MAIN
#include "frm_int.hpp"

namespace {

int screenWidth = 640;
int screenHeight = 480;
int bytesPerPixel = 4;
int ticksPerSecond = 60;

Uint64 performanceFrequency = 0;
Uint64 tickIntervalTicks = 0;
Uint64 nextTickTime = 0;

SDL_Window *window = nullptr;
SDL_Renderer *renderer = nullptr;
SDL_Texture *frameTexture = nullptr;
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

void waitUntilNextTickBoundary() {
  for (;;) {
    Uint64 now = SDL_GetPerformanceCounter();
    if (now >= nextTickTime) {
      break;
    }
    Uint64 remaining = nextTickTime - now;
    Uint64 remainingNs = (remaining * 1000000000ULL) / performanceFrequency;
    if (remainingNs > 2000000ULL) {
      SDL_Delay(1);
    }
  }
}

void advanceTickSchedule() {
  Uint64 now = SDL_GetPerformanceCounter();
  nextTickTime += tickIntervalTicks;
  if (now > nextTickTime + tickIntervalTicks) {
    nextTickTime = now + tickIntervalTicks;
  }
}

bool initSDL() {
  if (!SDL_Init(SDL_INIT_VIDEO | SDL_INIT_AUDIO)) {
    fprintf(stderr, "SDL_Init failed: %s\n", SDL_GetError());
    return false;
  }
  performanceFrequency = SDL_GetPerformanceFrequency();
  tickIntervalTicks = performanceFrequency / (Uint64)ticksPerSecond;
  nextTickTime = SDL_GetPerformanceCounter();

  window = SDL_CreateWindow("moonchild shell",
                            screenWidth, screenHeight, 0);
  if (!window) {
    fprintf(stderr, "SDL_CreateWindow failed: %s\n", SDL_GetError());
    return false;
  }

  renderer = SDL_CreateRenderer(window, "");
  if (!renderer) {
    fprintf(stderr, "SDL_CreateRenderer failed: %s\n", SDL_GetError());
    return false;
  }

  frameTexture =
      SDL_CreateTexture(renderer, SDL_PIXELFORMAT_BGRA32, SDL_TEXTUREACCESS_STREAMING,
                        screenWidth, screenHeight);
  if (!frameTexture) {
    fprintf(stderr, "SDL_CreateTexture failed: %s\n", SDL_GetError());
    return false;
  }
  SDL_SetTextureBlendMode(frameTexture, SDL_BLENDMODE_NONE);

  pixelBufferPitch = screenWidth * bytesPerPixel;
  pixelBuffer = new uint8_t[pixelBufferPitch * screenHeight];
  memset(pixelBuffer, 0, pixelBufferPitch * screenHeight);

  moviePlayer = new MoviePlayer();

  return true;
}

void shutdownSDL() {
  if (moviePlayer) {
    moviePlayer->stop(false);
    delete moviePlayer;
    moviePlayer = nullptr;
  }

  delete[] pixelBuffer;
  pixelBuffer = nullptr;

  if (frameTexture) {
    SDL_DestroyTexture(frameTexture);
    frameTexture = nullptr;
  }
  if (renderer) {
    SDL_DestroyRenderer(renderer);
    renderer = nullptr;
  }
  if (window) {
    SDL_DestroyWindow(window);
    window = nullptr;
  }
  SDL_Quit();
}

void presentFrame() {
  SDL_UpdateTexture(frameTexture, nullptr, pixelBuffer, pixelBufferPitch);
  SDL_RenderClear(renderer);
  SDL_RenderTexture(renderer, frameTexture, nullptr, nullptr);
  SDL_RenderPresent(renderer);
}

void syncMouse() {
  static int prevLeft = 0;
  float mx = 0;
  float my = 0;
  Uint32 buttons = SDL_GetMouseState(&mx, &my);
  g_MouseXCurrent = mx;
  g_MouseYCurrent = my;
  int left = (buttons & SDL_BUTTON_LMASK) ? 1 : 0;
  g_MouseFlg = left;
  g_MouseActualFlg = left;
  if (left != 0 && prevLeft == 0) {
    g_MouseXDown = mx;
    g_MouseYDown = my;
  }
  prevLeft = left;
}

}  // namespace


#if defined __DJGPP__
#define _Noreturn [[noreturn]]
#elif defined __WATCOMC__
#define _Noreturn __declspec(aborts)
#else
#error unsupported compiler
#endif


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


int main(int argc, char **argv) {
  (void)argc;
  (void)argv;

  if (!initSDL()) {
    shutdownSDL();
    return 1;
  }
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
    Uint8 *keyboardState = (Uint8 *)SDL_GetKeyboardState(&keyCount);
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
