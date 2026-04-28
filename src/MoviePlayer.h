#pragma once

#include <stdint.h>

typedef void (*MovieDoneCallback)(bool naturalEnd, void *userData);

class MoviePlayer {
 public:
  MoviePlayer();
  ~MoviePlayer();

  bool playFile(char *filePath, MovieDoneCallback doneCallback, void *userData);
  void stop(bool naturalEnd);
  bool isPlaying();
  void update(uint8_t *pixels, int width, int height, int pitch);

 private:
  void clearState();
  bool decodeNextFrame();
  void presentCurrentFrame(uint8_t *pixels, int width, int height, int pitch);
  void invokeDoneCallback(bool naturalEnd);

  struct Impl;
  Impl *impl;
  MovieDoneCallback doneCallback;
  void *doneUserData;
  bool playing;
  bool streamEnded;
  double movieDurationSec;
  uint64_t movieStartTicksMs;
  double lastPresentedPtsSec;
};
