#include <string>
#include <iostream>
#include <vector>

#include <boost/program_options.hpp>
#include <boost/format.hpp>
#include <boost/algorithm/string.hpp>
#include <AL/al.h>
#include <AL/alc.h>
#include <RtMidi.h>
#define _USE_MATH_DEFINES
#include <cmath>
#include <limits>
#include <mutex>
#include <chrono>
#include <thread>
#include <fstream>
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
size_t lastPitch = 0;

const std::vector<string> NOTE_LUT = { "C", "C#", "D", "D#", "E", "F", "F#", "G", "G#", "A", "A#", "B" };
typedef std::vector<double> AudioWindow;
AudioWindow audio_buffer;


void findDominantPitch(const vector<double>& source, size_t sampleRate) {
  using namespace Aquila;
  vector<double> target;
  const std::size_t SIZE = source.size();
	const double A1 = 440;
	HammingWindow hamming(SIZE);
  SignalSource in(source, sampleRate);
  FramesCollection frames(in,SIZE);
  auto signalFFT = FftFactory::getFft(SIZE);
  TextPlot plot;
  plot.setTitle("Signal");
  //create a high pass filter to remove low frequency noise
  const Aquila::FrequencyType f_hp = 200;
  Aquila::SpectrumType filterSpectrum(SIZE);
	for (std::size_t i = 0; i < SIZE; ++i) {
		if (i < (SIZE * f_hp / sampleRate)) {
			filterSpectrum[i] = 0.0;
		} else {
			filterSpectrum[i] = 1.0; //((double)SIZE - i) / SIZE;
		}
	}

//	plot.plotSpectrum(filterSpectrum);

  for (auto frame : frames) {
  	frame *= hamming;

		SpectrumType signalSpectrum = signalFFT->fft(frame.toArray());

    std::transform(
        std::begin(signalSpectrum),
        std::end(signalSpectrum),
        std::begin(filterSpectrum),
        std::begin(signalSpectrum),
        [] (Aquila::ComplexType x, Aquila::ComplexType y) { return x * y; }
    );

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

      const double& freq = (maxJ * sampleRate) / (double)SIZE;

    	size_t p = round(73.0 + 12.0 * log2(freq / A1));

      if(p > 40 && freq < 22050 && maxMag > 0.12) {
      	if(p != lastPitch) {
					const size_t octave = floor(p / 12.0);
					const string note = NOTE_LUT[p % 12];

					message.clear();
					message.push_back(0x80);
					message.push_back(lastPitch + 11);
					message.push_back(0);
					midiout->sendMessage(&message);

					//std::this_thread::sleep_for(std::chrono::milliseconds(200));
					message.clear();
					message.push_back(0x90);
					message.push_back(p + 11);
					message.push_back(0x1F);
					midiout->sendMessage(&message);
					//std::this_thread::sleep_for(std::chrono::milliseconds(200));

					std::cout << note << octave << '\t' << p << '\t' << maxMag << std::endl << std::flush;
					lastPitch = p;
      	}
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
  RecorderCallback rc = [=](AudioWindow& buffer) {
    findDominantPitch(buffer, sampleRate);
  };

  Recorder recorder(rc, bufferSize, sampleRate);
  recorder.capture(false);
}

int main(int argc, char** argv) {
  string audioFile;
  size_t bufferSize = 1024;
  uint32_t sampleRate = 44100;
  uint16_t midiPort = 0;
  uint16_t audioDevice = 0;
  po::options_description genericDesc("Options");
  genericDesc.add_options()("help,h", "Produce help message")
		("buffersize,b", po::value<size_t>(&bufferSize)->default_value(bufferSize),"The internal audio buffer size")
		("samplerate,s", po::value<uint32_t>(&sampleRate)->default_value(sampleRate),"The sample rate to record with")
		("midiport,m", po::value<uint16_t>(&midiPort)->default_value(midiPort),"The midi port to send messages to")
		("audiodev,a", po::value<uint16_t>(&audioDevice)->default_value(audioDevice),"The audio device to capture from")
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
