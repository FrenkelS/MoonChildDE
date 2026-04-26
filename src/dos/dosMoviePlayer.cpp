#include "MoviePlayer.h"


#define IMPLEMENT_ME() I_Error("Implement me: %s: %i\n", __FILE__, __LINE__);

[[noreturn]] void I_Error(const char *error, ...);


MoviePlayer::MoviePlayer() {
}


MoviePlayer::~MoviePlayer() {
}


bool MoviePlayer::isPlaying() {
  return false;
}


bool MoviePlayer::playFile(char *filePath, MovieDoneCallback callback, void *userData) {
  return false;
}


void MoviePlayer::stop(bool naturalEnd) {
}


void MoviePlayer::update(uint8_t *pixels, int width, int height, int pitch) {
  IMPLEMENT_ME();
}
