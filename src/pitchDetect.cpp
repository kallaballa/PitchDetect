#include <string>
#include <iostream>
#include <vector>
#include <sndfile.hh>
#include <boost/program_options.hpp>
#include <boost/format.hpp>
#include <boost/algorithm/string.hpp>
#include <samplerate.h>
#include <rtmidi/RtMidi.h>
#define _USE_MATH_DEFINES
#include <cmath>
#include <limits>
#include <mutex>
#include <chrono>
#include <thread>
#include "aquila/global.h"
#include "aquila/functions.h"
#include "aquila/transform/FftFactory.h"
#include "aquila/source/FramesCollection.h"
#include "aquila/tools/TextPlot.h"
#include "jackrecorder.hpp"


namespace po = boost::program_options;

using std::string;
using std::cerr;
using std::endl;
using std::vector;
RtMidiOut *midiout = new RtMidiOut();
std::vector<uint8_t> message;

const std::vector<string> NOTE_LUT = { "C", "C#", "D", "D#", "E", "F", "F#", "G", "G#", "A", "A#", "B" };
typedef std::vector<double> AudioWindow;
AudioWindow audio_buffer;

AudioWindow mix_channels(AudioWindow buffer, size_t channels) {
	AudioWindow mixed;
  for(size_t i = 0; i < buffer.size(); i+=channels) {
    double avg = 0;
  	for(size_t j = 0; j < channels; ++j) {
  	  avg += buffer[i + j];
  	}
  	avg /= channels;
  	mixed.push_back(avg);
  }

  return mixed;
}

std::vector<double> resample(vector<double> data, double sourceSampleRate, double targetSampleRate) {
  double sourceRatio = targetSampleRate / sourceSampleRate;
  size_t sourceSize = data.size();
  size_t targetSize = (sourceSize * sourceRatio) + 1;
  float* f_source = new float[sourceSize];
  float* f_target = new float[targetSize];

  std::vector<double> d_target;

  for(size_t i = 0; i < data.size(); ++i) {
    f_source[i] = data[i];
  }

  SRC_DATA src_data;

  src_data.data_in = f_source;
  src_data.input_frames = sourceSize;
  src_data.data_out = f_target;
  src_data.output_frames = targetSize;
  src_data.src_ratio = sourceRatio;
  src_data.end_of_input = 1;

  src_simple(&src_data, SRC_SINC_BEST_QUALITY, 1);

  for(size_t i = 0; i < targetSize; ++i) {
    d_target.push_back(f_target[i]);
  }


  delete[] f_source;
  delete[] f_target;

  return d_target;
}

void findDominantPitch(vector<double>& source, size_t sampleRate) {
	//midiout->ignoreTypes( false, false, false );
  using namespace Aquila;
  vector<double> target;
  const std::size_t SIZE = 1024;
	const double A1 = 440;
  size_t lastPitch = 0;

  SignalSource in(source, sampleRate);
  FramesCollection frames(in,SIZE);
  auto signalFFT = FftFactory::getFft(SIZE);

  for (auto frame : frames) {

    SpectrumType signalSpectrum = signalFFT->fft(frame.toArray());
    for (std::size_t i = 0; i < SIZE; ++i) {
    	double maxMag = 0;
    	size_t maxJ = 0;
    	double totalMag = 0;

      for(size_t j = 0; j < SIZE; ++j) {
      	const auto& c = signalSpectrum[j];
      	const auto& mag = abs(c);
      	if(mag > maxMag) {
      		maxMag = mag;
      		maxJ = j;
      	}
      	totalMag += mag;
      }
      const double& freq = ((maxJ + 1) * sampleRate) / SIZE;

    	const size_t p = round(68 + 12.0 * log2(freq / A1));
      if(p != lastPitch && p > 4 && freq < 22050 && maxMag > 50) {
      	const size_t octave = floor(p / 12.0);
      	const string note = NOTE_LUT[p % 12];

      	message.clear();
      	message.push_back(0x80);
      	message.push_back(64);
      	message.push_back(0);
      	midiout->sendMessage(&message);

      	message.clear();
      	message.push_back(0x90);
      	message.push_back(64);
      	message.push_back(0x80);
      	midiout->sendMessage(&message);

      	std::cout << note << octave << '\t' << totalMag << '\t' << maxMag << std::endl << std::flush;
        lastPitch = p;
      }
    }
  }
}

void normalize(std::vector<double>& data) {
  double min = std::numeric_limits<double>().max();
  double max = std::numeric_limits<double>().min();

  for (double& d : data) {
    min = std::min(min, d);
    max = std::max(max, d);
  }

  double delta = (max - min);
  double mid = min + (delta / 2);
  double scale = 2 / delta;

  for (double& d : data) {
    d = (d - mid) * scale;
  }
}

void run() {
  std::mutex bufferMutex;
  JackRecorder recorder("pitchDetect");
  double sourceSampleRate = recorder.getSampleRate();
  std::cerr << "SampleRate: " << sourceSampleRate << std::endl;
  JackRecorderCallback rc = [&](AudioWindow& buffer) {
    findDominantPitch(buffer, sourceSampleRate);
  };

  recorder.setCallback(rc);
  recorder.capture(false);
}

int main(int argc, char** argv) {
  /*
   * All distance options are in millimeter.
   */
  string audioFile;
  unsigned int nPorts = midiout->getPortCount();
  if ( nPorts == 0 ) {
    std::cout << "No ports available!\n";
    exit(1);
  }
  std::cerr << "Number of midi ports: " << nPorts << std::endl;
  std::string portName;
    for ( unsigned int i=0; i<nPorts; i++ ) {
      try {
        portName = midiout->getPortName(i);
      }
      catch ( RtMidiError &error ) {
        error.printMessage();
      }
      std::cout << "  Input Port #" << i << ": " << portName << '\n';
    }

	midiout->openVirtualPort( "virt");
//	assert(midiout->isPortOpen());
  po::options_description genericDesc("Options");
  genericDesc.add_options()("help,h", "Produce help message");

  po::options_description hidden("Hidden options");
  hidden.add_options()("audioFile", po::value<string>(&audioFile), "audioFile");

  po::options_description cmdline_options;
  cmdline_options.add(genericDesc).add(hidden);

  po::positional_options_description p;
  p.add("audioFile", -1);

  po::options_description visible;
  visible.add(genericDesc);

  po::variables_map vm;
  po::store(po::command_line_parser(argc, argv).options(cmdline_options).positional(p).run(), vm);
  po::notify(vm);

  if (vm.count("help")) {
    std::cerr << "Usage: pitchDetect [options]" << std::endl;
    std::cerr << visible;
    return 0;
  }

  run();

  return 0;
}
