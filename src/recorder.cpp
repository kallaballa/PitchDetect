#include "recorder.hpp"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <thread>

#include <sys/time.h>
#include <ctime>

using std::cerr;
using std::endl;

static void list_audio_devices(const ALCchar *devices) {
	const ALCchar *device = devices, *next = devices + 1;
	size_t len = 0;

	fprintf(stdout, "Devices list:\n");
	fprintf(stdout, "----------\n");
	while (device && *device != '\0' && next && *next != '\0') {
		fprintf(stdout, "%s\n", device);
		len = strlen(device);
		device += (len + 1);
		next += (len + 2);
	}
	fprintf(stdout, "----------\n");
}

void displayDevices(const char *type, const char *list)
{
  ALCchar *ptr, *nptr;

  ptr = (ALCchar *)list;
  printf("list of all available %s devices:\n", type);
  if (!list)
  {
    printf("none\n");
  }
  else
  {
    nptr = ptr;
    while (*(nptr += strlen(ptr)+1) != 0)
    {
      printf("  %s\n", ptr);
      ptr = nptr;
    }
    printf("  %s\n", ptr);
  }
}
Recorder::Recorder(RecorderCallback callback, uint32_t sampleRate, uint32_t samplesPerFrame) :
    callback_(callback),
    sampleRate_(sampleRate),
    samplesPerFrame_(samplesPerFrame){
  const ALCchar * devices;

  char * s = (char *)alcGetString(NULL, ALC_CAPTURE_DEVICE_SPECIFIER);
  displayDevices("input", s);
  std::cerr << alcGetString(NULL, ALC_DEFAULT_DEVICE_SPECIFIER) << std::endl;

  std::cerr << "Opening capture device:" << std::endl;
  captureDev_ = alcCaptureOpenDevice(NULL, sampleRate, AL_FORMAT_MONO16, samplesPerFrame);
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
    alcGetIntegerv(captureDev_, ALC_CAPTURE_SAMPLES, 1, &samplesAvailable);

    if (samplesAvailable > 0) {
      alcCaptureSamples(captureDev_, captureBuffer, samplesAvailable);

      for(size_t i = 0; i < (size_t)samplesAvailable; i+=2) {
        if(buffer.size() >= samplesPerFrame_) {
          callback_(buffer);
          buffer.clear();
        }
        short sample = captureBuffer[i];
        sample = sample | (((short)captureBuffer[i + 1]) << 8);
        buffer.push_back(((double)sample) / ((double)std::numeric_limits<short>().max()));
      }
    }

    usleep(10000);
  }
  });
  if(detach)
  	captureThread.detach();
  else
  	captureThread.join();
}

