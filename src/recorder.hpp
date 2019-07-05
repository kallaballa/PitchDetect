#ifndef SRC_RECORDER_HPP_
#define SRC_RECORDER_HPP_
#include <iostream>
#include <cmath>
#include <cassert>
#include <cstddef>
#include <functional>
#include <vector>
#include <AL/al.h>
#include <AL/alc.h>

typedef std::function<void(std::vector<double>&)> RecorderCallback;
class Recorder {
  ALCdevice * captureDev_;
  RecorderCallback callback_;
  size_t bufferSize_;
  uint32_t sampleRate_;
  ALubyte captureBuffer[1048576];
  ALint samplesAvailable = 0;
  std::vector<double> buffer;
public:
  Recorder(RecorderCallback callback, size_t bufferSize, uint32_t sampleRate);
  virtual ~Recorder();
  void capture(bool detach = true);
  std::vector<std::string> list();

};

#endif /* SRC_RECORDER_HPP_ */
