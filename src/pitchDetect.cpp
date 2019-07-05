#include <string>
#include <iostream>
#include <vector>
#include <sndfile.hh>
#include <boost/program_options.hpp>
#include <boost/format.hpp>
#include <boost/algorithm/string.hpp>
#include <samplerate.h>
#include <AL/al.h>
#include <AL/alc.h>
#include <RtMidi.h>
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
#include "aquila/source/window/HammingWindow.h"
#include "recorder.hpp"


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

size_t lastPitch = 0;

void findDominantPitch(const vector<double>& source, size_t sampleRate) {
  using namespace Aquila;
  vector<double> target;
  const std::size_t SIZE = source.size();
	const double A1 = 440;
	HammingWindow hamming(SIZE);
  SignalSource in(source, sampleRate);
  FramesCollection frames(in,SIZE);
  auto signalFFT = FftFactory::getFft(SIZE);

  for (auto frame : frames) {
  	frame *= hamming;

		SpectrumType signalSpectrum = signalFFT->fft(frame.toArray());
		for (std::size_t i = 0; i < SIZE; ++i) {
			double maxMag = 0;
			size_t maxJ = 0;
			double totalMag = 0;

			for (size_t j = 0; j < SIZE; ++j) {
				auto& c = signalSpectrum[j];

				const auto& mag = abs(c) / SIZE;
				if (mag > maxMag) {
					maxMag = mag;
					maxJ = j;
				}
				totalMag += mag;
			}

//			double sum = 0.0, mean, variance = 0.0, stdDeviation;
//
//			for (size_t i = 0; i < SIZE; ++i)
//				sum += abs(signalSpectrum[i]);
//			mean = sum / SIZE;
//
//			for (size_t i = 0; i < SIZE; ++i)
//				variance += pow(abs(signalSpectrum[i]) - mean, 2);
//			variance = variance / SIZE;
//
//			stdDeviation = sqrt(variance);

      const double& freq = ((maxJ + 1) * sampleRate) / SIZE;

    	const size_t p = round(68 + 12.0 * log2(freq / A1));

      if(p != lastPitch && freq < 22050 && maxMag > 0.001) {
      	const size_t octave = floor(p / 12.0);
      	const string note = NOTE_LUT[p % 12];

      	message.clear();
      	message.push_back(0x80);
      	message.push_back(lastPitch + 12);
      	message.push_back(0);
      	midiout->sendMessage(&message);

      	//std::this_thread::sleep_for(std::chrono::milliseconds(200));
      	message.clear();
      	message.push_back(0x90);
      	message.push_back(p + 12);
      	message.push_back(0x1F);
      	midiout->sendMessage(&message);
      	//std::this_thread::sleep_for(std::chrono::milliseconds(200));

      	std::cout << note << octave << '\t' << p << '\t' << maxMag << std::endl << std::flush;
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

void run(size_t bufferSize, uint32_t sampleRate) {
  std::mutex bufferMutex;
  RecorderCallback rc = [&](AudioWindow& buffer) {
    findDominantPitch(buffer, sampleRate);
  };

  Recorder recorder(rc, bufferSize, sampleRate);
  recorder.capture(false);
}

int main(int argc, char** argv) {
  string audioFile;
  size_t bufferSize = 1024;
  uint32_t sampleRate = 44100;
  uint16_t midiPort = 1;

  po::options_description genericDesc("Options");
  genericDesc.add_options()("help,h", "Produce help message")
		("buffersize,b", po::value<size_t>(&bufferSize)->default_value(bufferSize),"The internal audio buffer size")
		("samplerate,s", po::value<uint32_t>(&sampleRate)->default_value(sampleRate),"The sample rate to record with")
		("midiport,p", po::value<uint16_t>(&midiPort)->default_value(midiPort),"The midi port to send messages to")
		("list,l", "List midi ports and audio devices");


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
  if(vm.count("list")) {
		unsigned int nPorts = midiout->getPortCount();
		const auto captureDevices = Recorder::list();
		if (nPorts == 0) {
			std::cerr << "No ports available!\n";
			exit(1);
		}
		std::cerr << "Number of midi ports: " << nPorts << std::endl;
		std::string portName;
		for (unsigned int i = 0; i < nPorts; i++) {
			try {
				portName = midiout->getPortName(i);
			} catch (RtMidiError &error) {
				error.printMessage();
			}
			std::cerr << "  Output Port #" << i << ": " << portName << '\n';
		}

		std::cerr << "Number of capture devices: " << captureDevices.size() << std::endl;
		size_t i = 0;
		for (const string& device : captureDevices) {
			std::cerr << "  Capture device# " << i++ << ": " << device << '\n';
		}

		exit(0);
  }
	midiout->openPort(midiPort);
	assert(midiout->isPortOpen());
  run(bufferSize, sampleRate);

  return 0;
}
