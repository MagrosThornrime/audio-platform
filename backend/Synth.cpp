#include <portaudiocpp/PortAudioCpp.hxx>
#include <iostream>
#include <thread>
#include <chrono>
#include <oscillators/SawtoothOscillator.hpp>
#include <oscillators/SineOscillator.hpp>
#include <oscillators/SquareOscillator.hpp>
#include <oscillators/TriangleOscillator.hpp>
#include <oscillators/NoiseOscillator.hpp>
#include <Midi.hpp>
#include <fmt/core.h>

int main() {
	try {
		portaudio::AutoSystem autoSys;

		portaudio::System& sys = portaudio::System::instance();

		portaudio::Device& outputDevice = sys.defaultOutputDevice();

		std::cout << "Using output device: " << outputDevice.name() << std::endl;

		portaudio::DirectionSpecificStreamParameters outParams(
			outputDevice,
			2,                      // 2 channels (stereo)
			portaudio::FLOAT32,     // sample format
			true,                  // interleaved
			outputDevice.defaultLowOutputLatency(),
			nullptr
		);

		portaudio::StreamParameters streamParams(
			portaudio::DirectionSpecificStreamParameters::null(), // input
			outParams,
			44100,
			256,
			paClipOff
		);

		SineOscillator oscillator(44100.0f);
		oscillator.setFrequency(0);
		//oscillator.setAmplitude(0.05f);

		// Use BlockingStream instead of MemFunCallbackStream
		portaudio::MemFunCallbackStream<Oscillator> stream(
			streamParams,
			oscillator,
			&SineOscillator::paCallback
		);

		stream.start();
		std::cout << "Stream started." << std::endl;

		{
			for (auto&& port : midi::Ports::list()) {
				fmt::println("{}: {}", port.num, port.name);
			}
			fmt::print("> ");
			u32 which{};
			std::cin >> which;
			std::cin.ignore();

			auto midiThread = std::jthread([&](std::stop_token stopToken) {
				try {
					midi::Reader reader;
					reader.open(midi::Ports::getByNum(which));
					reader.readAll();

					for (; not stopToken.stop_requested();) {
						auto data = reader.read();
						if (data.hasVelocity()) {
							auto note = data.note();
							fmt::print("{}{}", note.name, data.status() == midi::Data::noteOn ? 'v' : '^');
							if (data.status() == midi::Data::noteOn) {
								oscillator.setAmplitude(1);
								oscillator.setFrequency(note.freq);
								fmt::println(" {}", data.velocity());
							}
							if (data.status() == midi::Data::noteOff) {
								oscillator.setAmplitude(0);
								fmt::println("");
							}
						}
					}
				} catch (std::exception& e) {
					fmt::println("{}", e.what());
				}
			});

			(void)getchar();
		}

		stream.stop();
		stream.close();
		std::cout << "Stream stopped and closed." << std::endl;

	} catch (const portaudio::PaException& e) {
		std::cerr << "PortAudio exception: " << e.paErrorText() << std::endl;
		return 1;
	} catch (const std::exception& e) {
		std::cerr << "Standard exception: " << e.what() << std::endl;
		return 1;
	}

	return 0;
}
