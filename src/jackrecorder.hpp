#ifndef SRC_RECORDER_HPP_
#define SRC_RECORDER_HPP_
#include <iostream>
#include <cmath>
#include <cassert>
#include <cstddef>
#include <functional>
#include <vector>
#include <getopt.h>
#include <jack/jack.h>
#include <jack/ringbuffer.h>

typedef std::function<void(std::vector<double>&)> JackRecorderCallback;

typedef struct _thread_info {
    pthread_t thread_id;
    jack_nframes_t duration;
    jack_nframes_t rb_size;
    jack_client_t *client;
    unsigned int channels;
    int bitdepth;
    char *path;
    volatile int can_capture;
    volatile int can_process;
    volatile int status;
    JackRecorderCallback callback;
} jack_thread_info_t;

class JackRecorder {
private:
  JackRecorderCallback callback_;
  void jack_shutdown(void *arg);
  void setup_disk_thread (jack_thread_info_t *info);
  void run_disk_thread (jack_thread_info_t *info);
  void setup_ports (int sources, char *source_names[], jack_thread_info_t *info);

public:
	JackRecorder(const std::string& name);
  virtual ~JackRecorder();
  void setCallback(JackRecorderCallback& callback);
  void capture(bool detach = true);
  size_t getSampleRate();

  std::vector<std::string> list();

};

#endif /* SRC_RECORDER_HPP_ */
