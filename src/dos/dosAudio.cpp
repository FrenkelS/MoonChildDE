#include "Audio.h"


#define IMPLEMENT_ME() I_Error("Implement me: %s: %i\n", __FILE__, __LINE__);

[[noreturn]] void I_Error(const char *error, ...);


bool initAudio() {
  return true;
}


bool loadMusicFile(char *path) {
  return false;
}


void playMusicLooping() {
}


void stopMusic() {
}


void volumeWaveSample(int assetHandle, int volume) {
}


void shutdownAudio() {
}


int loadWaveSample(char *path) {
  IMPLEMENT_ME();
  return 0;
}

void freeWaveSample(int assetHandle) {
  IMPLEMENT_ME();
}

int playWaveOneshot(int assetHandle) {
  IMPLEMENT_ME();
  return 0;
}

int playWaveLooping(int assetHandle) {
  IMPLEMENT_ME();
  return 0;
}

void stopWaveSample(int assetHandle) {
  IMPLEMENT_ME();
}

bool panWaveSample(int assetHandle, int left, int right) {
  IMPLEMENT_ME();
  return false;
}
