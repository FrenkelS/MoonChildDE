#include "MoviePlayer.h"

#include <SDL.h>

#include <algorithm>
#include <cstdio>
#include <cstring>
#include <vector>

extern "C" {
#include <libavcodec/avcodec.h>
#include <libavformat/avformat.h>
#include <libavutil/channel_layout.h>
#include <libavutil/imgutils.h>
#include <libavutil/rational.h>
#include <libavutil/samplefmt.h>
#include <libswresample/swresample.h>
#include <libswscale/swscale.h>
}

struct MoviePlayer::Impl {
  AVFormatContext *formatCtx;
  AVCodecContext *videoCodecCtx;
  AVCodecContext *audioCodecCtx;
  SwsContext *swsCtx;
  SwrContext *swrCtx;
  AVFrame *videoFrame;
  AVFrame *audioFrame;
  AVFrame *convertedVideoFrame;
  AVPacket *packet;
  int videoStreamIndex;
  int audioStreamIndex;
  int targetWidth;
  int targetHeight;
  uint8_t *convertedVideoBuffer;
  bool hasVideoFrame;
  bool demuxEnded;
  bool videoDecoderFlushed;
  bool audioDecoderFlushed;
  bool videoDecoderEnded;
  bool audioDecoderEnded;
  double currentVideoPtsSec;
  SDL_AudioDeviceID audioDevice;
  SDL_AudioSpec audioObtainedSpec;

  Impl() {
    formatCtx = nullptr;
    videoCodecCtx = nullptr;
    audioCodecCtx = nullptr;
    swsCtx = nullptr;
    swrCtx = nullptr;
    videoFrame = nullptr;
    audioFrame = nullptr;
    convertedVideoFrame = nullptr;
    packet = nullptr;
    videoStreamIndex = -1;
    audioStreamIndex = -1;
    targetWidth = 640;
    targetHeight = 480;
    convertedVideoBuffer = nullptr;
    hasVideoFrame = false;
    demuxEnded = false;
    videoDecoderFlushed = false;
    audioDecoderFlushed = false;
    videoDecoderEnded = false;
    audioDecoderEnded = false;
    currentVideoPtsSec = 0.0;
    audioDevice = 0;
    memset(&audioObtainedSpec, 0, sizeof(audioObtainedSpec));
  }
};

namespace {

double secondsFromTimeBase(int64_t pts, AVRational timeBase) {
  if (pts == AV_NOPTS_VALUE) {
    return -1.0;
  }
  return (double)pts * av_q2d(timeBase);
}

double fallbackFrameSeconds(AVCodecContext *videoCodecCtx) {
  if (!videoCodecCtx) {
    return 1.0 / 30.0;
  }
  if (videoCodecCtx->framerate.num > 0 && videoCodecCtx->framerate.den > 0) {
    return (double)videoCodecCtx->framerate.den / (double)videoCodecCtx->framerate.num;
  }
  return 1.0 / 30.0;
}

}  // namespace

MoviePlayer::MoviePlayer() {
  impl = new Impl();
  playing = false;
  streamEnded = false;
  movieDurationSec = 0.0;
  movieStartTicksMs = 0;
  lastPresentedPtsSec = -1.0;
}

MoviePlayer::~MoviePlayer() {
  clearState();
  delete impl;
  impl = nullptr;
}

void MoviePlayer::clearState() {
  if (!impl) {
    return;
  }

  if (impl->audioDevice != 0) {
    SDL_ClearQueuedAudio(impl->audioDevice);
    SDL_CloseAudioDevice(impl->audioDevice);
    impl->audioDevice = 0;
  }

  if (impl->packet) {
    av_packet_free(&impl->packet);
  }
  if (impl->convertedVideoFrame) {
    av_frame_free(&impl->convertedVideoFrame);
  }
  if (impl->videoFrame) {
    av_frame_free(&impl->videoFrame);
  }
  if (impl->audioFrame) {
    av_frame_free(&impl->audioFrame);
  }
  if (impl->convertedVideoBuffer) {
    av_free(impl->convertedVideoBuffer);
    impl->convertedVideoBuffer = nullptr;
  }
  if (impl->swsCtx) {
    sws_freeContext(impl->swsCtx);
    impl->swsCtx = nullptr;
  }
  if (impl->swrCtx) {
    swr_free(&impl->swrCtx);
  }
  if (impl->videoCodecCtx) {
    avcodec_free_context(&impl->videoCodecCtx);
  }
  if (impl->audioCodecCtx) {
    avcodec_free_context(&impl->audioCodecCtx);
  }
  if (impl->formatCtx) {
    avformat_close_input(&impl->formatCtx);
  }

  impl->videoStreamIndex = -1;
  impl->audioStreamIndex = -1;
  impl->hasVideoFrame = false;
  impl->demuxEnded = false;
  impl->videoDecoderFlushed = false;
  impl->audioDecoderFlushed = false;
  impl->videoDecoderEnded = false;
  impl->audioDecoderEnded = false;
  impl->currentVideoPtsSec = 0.0;
  memset(&impl->audioObtainedSpec, 0, sizeof(impl->audioObtainedSpec));

  playing = false;
  streamEnded = false;
  movieDurationSec = 0.0;
  movieStartTicksMs = 0;
  lastPresentedPtsSec = -1.0;
}

bool MoviePlayer::playFile(char *filePath, MovieDoneCallback callback, void *userData) {
  clearState();

  doneCallback = callback;
  doneUserData = userData;

  if (avformat_open_input(&impl->formatCtx, filePath, nullptr, nullptr) < 0) {
    fprintf(stderr, "movie: could not open %s\n", filePath);
    clearState();
    return false;
  }
  if (avformat_find_stream_info(impl->formatCtx, nullptr) < 0) {
    fprintf(stderr, "movie: could not read stream info for %s\n", filePath);
    clearState();
    return false;
  }

  impl->videoStreamIndex =
      av_find_best_stream(impl->formatCtx, AVMEDIA_TYPE_VIDEO, -1, -1, nullptr, 0);
  if (impl->videoStreamIndex < 0) {
    fprintf(stderr, "movie: no video stream in %s\n", filePath);
    clearState();
    return false;
  }

  AVStream *videoStream = impl->formatCtx->streams[impl->videoStreamIndex];
  AVCodecParameters *videoCodecParams = videoStream->codecpar;
  AVCodec *videoDecoder = (AVCodec *)avcodec_find_decoder(videoCodecParams->codec_id);
  if (!videoDecoder) {
    fprintf(stderr, "movie: unsupported video codec in %s\n", filePath);
    clearState();
    return false;
  }

  impl->videoCodecCtx = avcodec_alloc_context3(videoDecoder);
  if (!impl->videoCodecCtx) {
    clearState();
    return false;
  }
  if (avcodec_parameters_to_context(impl->videoCodecCtx, videoCodecParams) < 0) {
    clearState();
    return false;
  }
  if (avcodec_open2(impl->videoCodecCtx, videoDecoder, nullptr) < 0) {
    clearState();
    return false;
  }

  impl->audioStreamIndex =
      av_find_best_stream(impl->formatCtx, AVMEDIA_TYPE_AUDIO, -1, -1, nullptr, 0);
  if (impl->audioStreamIndex >= 0) {
    AVStream *audioStream = impl->formatCtx->streams[impl->audioStreamIndex];
    AVCodecParameters *audioCodecParams = audioStream->codecpar;
    AVCodec *audioDecoder = (AVCodec *)avcodec_find_decoder(audioCodecParams->codec_id);
    if (audioDecoder) {
      impl->audioCodecCtx = avcodec_alloc_context3(audioDecoder);
      if (!impl->audioCodecCtx) {
        clearState();
        return false;
      }
      if (avcodec_parameters_to_context(impl->audioCodecCtx, audioCodecParams) < 0) {
        clearState();
        return false;
      }
      if (avcodec_open2(impl->audioCodecCtx, audioDecoder, nullptr) < 0) {
        avcodec_free_context(&impl->audioCodecCtx);
      }
    }
  }

  impl->videoFrame = av_frame_alloc();
  impl->audioFrame = av_frame_alloc();
  impl->convertedVideoFrame = av_frame_alloc();
  impl->packet = av_packet_alloc();
  if (!impl->videoFrame || !impl->audioFrame || !impl->convertedVideoFrame || !impl->packet) {
    clearState();
    return false;
  }

  int bufferSize = av_image_get_buffer_size(AV_PIX_FMT_BGRA, impl->targetWidth,
                                            impl->targetHeight, 1);
  if (bufferSize < 0) {
    clearState();
    return false;
  }
  impl->convertedVideoBuffer = (uint8_t *)av_malloc((size_t)bufferSize);
  if (!impl->convertedVideoBuffer) {
    clearState();
    return false;
  }

  if (av_image_fill_arrays(impl->convertedVideoFrame->data,
                           impl->convertedVideoFrame->linesize, impl->convertedVideoBuffer,
                           AV_PIX_FMT_BGRA, impl->targetWidth, impl->targetHeight, 1) < 0) {
    clearState();
    return false;
  }

  impl->swsCtx = sws_getContext(impl->videoCodecCtx->width, impl->videoCodecCtx->height,
                                impl->videoCodecCtx->pix_fmt, impl->targetWidth,
                                impl->targetHeight, AV_PIX_FMT_BGRA, SWS_BILINEAR, nullptr,
                                nullptr, nullptr);
  if (!impl->swsCtx) {
    clearState();
    return false;
  }

  if (impl->audioCodecCtx) {
    SDL_AudioSpec desiredSpec;
    memset(&desiredSpec, 0, sizeof(desiredSpec));
    desiredSpec.freq = impl->audioCodecCtx->sample_rate > 0 ? impl->audioCodecCtx->sample_rate : 48000;
    desiredSpec.format = AUDIO_S16SYS;
    desiredSpec.channels = 2;
    desiredSpec.samples = 2048;
    desiredSpec.callback = nullptr;

    impl->audioDevice = SDL_OpenAudioDevice(nullptr, 0, &desiredSpec, &impl->audioObtainedSpec, 0);
    if (impl->audioDevice != 0) {
      AVChannelLayout outputLayout;
      av_channel_layout_default(&outputLayout, impl->audioObtainedSpec.channels);

      if (swr_alloc_set_opts2(&impl->swrCtx, &outputLayout, AV_SAMPLE_FMT_S16,
                              impl->audioObtainedSpec.freq, &impl->audioCodecCtx->ch_layout,
                              impl->audioCodecCtx->sample_fmt,
                              impl->audioCodecCtx->sample_rate, 0, nullptr) < 0) {
        av_channel_layout_uninit(&outputLayout);
        clearState();
        return false;
      }
      av_channel_layout_uninit(&outputLayout);

      if (swr_init(impl->swrCtx) < 0) {
        clearState();
        return false;
      }

      SDL_PauseAudioDevice(impl->audioDevice, 0);
    } else {
      fprintf(stderr, "movie: could not open audio device: %s\n", SDL_GetError());
    }
  }

  if (impl->formatCtx->duration > 0) {
    movieDurationSec = (double)impl->formatCtx->duration / (double)AV_TIME_BASE;
  } else {
    movieDurationSec = 0.0;
  }

  movieStartTicksMs = SDL_GetTicks64();
  playing = true;
  streamEnded = false;
  impl->demuxEnded = false;
  impl->videoDecoderFlushed = false;
  impl->audioDecoderFlushed = false;
  impl->videoDecoderEnded = false;
  impl->audioDecoderEnded = impl->audioCodecCtx == nullptr;
  lastPresentedPtsSec = -1.0;

  if (!decodeNextFrame()) {
    stop(true);
    return false;
  }

  return true;
}

void MoviePlayer::invokeDoneCallback(bool naturalEnd) {
  MovieDoneCallback callback = doneCallback;
  void *userData = doneUserData;
  doneCallback = nullptr;
  doneUserData = nullptr;
  if (callback) {
    callback(naturalEnd, userData);
  }
}

void MoviePlayer::stop(bool naturalEnd) {
  if (!playing) {
    clearState();
    return;
  }
  clearState();
  invokeDoneCallback(naturalEnd);
}

bool MoviePlayer::isPlaying() {
  return playing;
}

bool MoviePlayer::decodeNextFrame() {
  if (!playing) {
    return false;
  }

  AVStream *videoStream = impl->formatCtx->streams[impl->videoStreamIndex];

  int maxWork = 48;
  while (maxWork-- > 0) {
    if (!impl->demuxEnded) {
      int readResult = av_read_frame(impl->formatCtx, impl->packet);
      if (readResult >= 0) {
        if (impl->packet->stream_index == impl->videoStreamIndex) {
          avcodec_send_packet(impl->videoCodecCtx, impl->packet);
        } else if (impl->audioCodecCtx && impl->packet->stream_index == impl->audioStreamIndex) {
          avcodec_send_packet(impl->audioCodecCtx, impl->packet);
        }
        av_packet_unref(impl->packet);
      } else if (readResult == AVERROR_EOF) {
        impl->demuxEnded = true;
        if (!impl->videoDecoderFlushed) {
          avcodec_send_packet(impl->videoCodecCtx, nullptr);
          impl->videoDecoderFlushed = true;
        }
        if (impl->audioCodecCtx && !impl->audioDecoderFlushed) {
          avcodec_send_packet(impl->audioCodecCtx, nullptr);
          impl->audioDecoderFlushed = true;
        }
      } else {
        return false;
      }
    }

    for (;;) {
      int receiveVideoResult = avcodec_receive_frame(impl->videoCodecCtx, impl->videoFrame);
      if (receiveVideoResult == AVERROR(EAGAIN)) {
        break;
      }
      if (receiveVideoResult == AVERROR_EOF) {
        impl->videoDecoderEnded = true;
        break;
      }
      if (receiveVideoResult < 0) {
        return false;
      }

      int64_t bestPts = impl->videoFrame->best_effort_timestamp;
      double ptsSec = secondsFromTimeBase(bestPts, videoStream->time_base);
      if (ptsSec < 0.0) {
        if (lastPresentedPtsSec >= 0.0) {
          ptsSec = lastPresentedPtsSec + fallbackFrameSeconds(impl->videoCodecCtx);
        } else {
          ptsSec = 0.0;
        }
      }

      sws_scale(impl->swsCtx, impl->videoFrame->data, impl->videoFrame->linesize, 0,
                impl->videoCodecCtx->height, impl->convertedVideoFrame->data,
                impl->convertedVideoFrame->linesize);

      impl->currentVideoPtsSec = ptsSec;
      impl->hasVideoFrame = true;
      return true;
    }

    if (impl->audioCodecCtx && impl->audioDevice != 0 && impl->swrCtx && !impl->audioDecoderEnded) {
      for (;;) {
        int receiveAudioResult = avcodec_receive_frame(impl->audioCodecCtx, impl->audioFrame);
        if (receiveAudioResult == AVERROR(EAGAIN)) {
          break;
        }
        if (receiveAudioResult == AVERROR_EOF) {
          impl->audioDecoderEnded = true;
          break;
        }
        if (receiveAudioResult < 0) {
          return false;
        }

        int inputRate = impl->audioCodecCtx->sample_rate;
        if (inputRate <= 0) {
          inputRate = impl->audioObtainedSpec.freq;
        }
        int outputSamples = (int)av_rescale_rnd(
            swr_get_delay(impl->swrCtx, inputRate) + impl->audioFrame->nb_samples,
            impl->audioObtainedSpec.freq, inputRate, AV_ROUND_UP);
        int outputChannels = impl->audioObtainedSpec.channels;
        int bufferSize = av_samples_get_buffer_size(nullptr, outputChannels, outputSamples,
                                                    AV_SAMPLE_FMT_S16, 1);
        if (bufferSize <= 0) {
          continue;
        }

        std::vector<uint8_t> outputBuffer((size_t)bufferSize);
        uint8_t *outputData[1];
        outputData[0] = outputBuffer.data();
        int convertedSamples = swr_convert(impl->swrCtx, outputData, outputSamples,
                                           (uint8_t **)impl->audioFrame->extended_data,
                                           impl->audioFrame->nb_samples);
        if (convertedSamples <= 0) {
          continue;
        }
        int bytesPerSample = av_get_bytes_per_sample(AV_SAMPLE_FMT_S16);
        int convertedBytes = convertedSamples * outputChannels * bytesPerSample;
        if (convertedBytes > 0) {
          SDL_QueueAudio(impl->audioDevice, outputBuffer.data(), (Uint32)convertedBytes);
        }
      }
    }

    if (impl->demuxEnded && impl->videoDecoderEnded) {
      return false;
    }
  }

  return impl->hasVideoFrame;
}

void MoviePlayer::presentCurrentFrame(uint8_t *pixels, int width, int height, int pitch) {
  int frameWidth = impl->targetWidth;
  int frameHeight = impl->targetHeight;
  int copyWidth = std::min(width, frameWidth);
  int copyHeight = std::min(height, frameHeight);
  int xOffset = (width - copyWidth) / 2;
  int yOffset = (height - copyHeight) / 2;

  memset(pixels, 0, (size_t)pitch * (size_t)height);

  uint8_t *srcBase = impl->convertedVideoFrame->data[0];
  int srcPitch = impl->convertedVideoFrame->linesize[0];
  for (int y = 0; y < copyHeight; y++) {
    uint8_t *src = srcBase + y * srcPitch;
    uint8_t *dst = pixels + (y + yOffset) * pitch + xOffset * 4;
    memcpy(dst, src, (size_t)copyWidth * 4);
  }
}

void MoviePlayer::update(uint8_t *pixels, int width, int height, int pitch) {
  if (!playing) {
    return;
  }

  double elapsedSec = (double)(SDL_GetTicks64() - movieStartTicksMs) / 1000.0;
  int workPass = 0;
  while (workPass < 6) {
    if (!impl->hasVideoFrame) {
      if (!decodeNextFrame()) {
        streamEnded = true;
        break;
      }
    }
    if (impl->hasVideoFrame && impl->currentVideoPtsSec <= elapsedSec + 0.0005) {
      presentCurrentFrame(pixels, width, height, pitch);
      lastPresentedPtsSec = impl->currentVideoPtsSec;
      impl->hasVideoFrame = false;
      workPass++;
      continue;
    }
    break;
  }

  if (!impl->hasVideoFrame && !streamEnded) {
    if (!decodeNextFrame()) {
      streamEnded = true;
    }
  }

  if (streamEnded || (impl->demuxEnded && impl->videoDecoderEnded)) {
    bool audioDrained = true;
    if (impl->audioDevice != 0) {
      audioDrained = SDL_GetQueuedAudioSize(impl->audioDevice) == 0;
    }
    if (audioDrained) {
      stop(true);
    }
  } else if (impl->hasVideoFrame && lastPresentedPtsSec < 0.0) {
    presentCurrentFrame(pixels, width, height, pitch);
    lastPresentedPtsSec = impl->currentVideoPtsSec;
  }
}
