#include "recorder.hpp"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <thread>
#include <sys/time.h>
#include <ctime>
#include <sndfile.hh>
#include <fstream>

std::ofstream dump("dump.wav");

using std::cerr;
using std::endl;

Recorder::Recorder(RecorderCallback callback, size_t bufferSize, uint32_t sampleRate) :
    callback_(callback),
		bufferSize_(bufferSize),
    sampleRate_(sampleRate) {
  const ALCchar * devices;

  std::cerr << alcGetString(NULL, ALC_DEFAULT_DEVICE_SPECIFIER) << std::endl;

  std::cerr << "Opening capture device:" << std::endl;
  captureDev_ = alcCaptureOpenDevice(NULL, sampleRate, AL_FORMAT_MONO8, 4096);
  if (captureDev_ == NULL) {
    std::cerr << "Unable to open device!:" << std::endl;
    exit(1);
  }
  devices = alcGetString(captureDev_, ALC_CAPTURE_DEVICE_SPECIFIER);
  std::cerr << "opened device" << devices << std::endl;
}

Recorder::~Recorder() {
  alcCaptureStop(captureDev_);
  alcCaptureCloseDevice(captureDev_);
}


std::vector<std::string> Recorder::list() {
  std::vector<std::string> result;
  const ALCchar * devices;
  const ALCchar * ptr;
  printf("Available capture devices:\n");
  devices = alcGetString(NULL, ALC_CAPTURE_DEVICE_SPECIFIER);
  ptr = devices;

  while (*ptr) {
    result.push_back(ptr);
    ptr += strlen(ptr) + 1;
  }
  return result;
}

void Recorder::capture(bool detach) {
  std::thread captureThread([&](){
  alcCaptureStart(captureDev_);
  while (true) {
    alcGetIntegerv(captureDev_, ALC_CAPTURE_SAMPLES, sizeof(ALint), &samplesAvailable);
    if (samplesAvailable > 1) {

    	alcCaptureSamples(captureDev_, captureBuffer, samplesAvailable);

      for(size_t i = 0; i < (size_t)samplesAvailable; i++) {
        if(buffer.size() >= bufferSize_) {
          callback_(buffer);
          buffer.clear();
        }
//
//        uint16_t sample = captureBuffer[i + 1];
//        sample = sample | (((uint32_t)captureBuffer[i]) << 8);
        dump << captureBuffer[i];
//        buffer.push_back((double)sample / std::numeric_limits<uint16_t>::max());
        buffer.push_back((double)captureBuffer[i]);

      }
    }

 //   usleep(10000);
  }
  });
  if(detach)
  	captureThread.detach();
  else
  	captureThread.join();
}

