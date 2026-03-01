#include "Audio.h"

Audio::Audio(const std::string& fileName)
	: audioFile(fileName, std::ios::binary)
{
	CoCreateInstance(__uuidof(MMDeviceEnumerator), nullptr, CLSCTX_ALL,
		__uuidof(IMMDeviceEnumerator), (void**)&enumerator);

	// Initialise clients / devices
	enumerator->GetDefaultAudioEndpoint(eRender, eConsole, &device);
	device->Activate(__uuidof(IAudioClient), CLSCTX_ALL, nullptr, (void**)&audioClient);
	audioClient->GetMixFormat(&fileFormat);
	audioClient->Initialize(
		AUDCLNT_SHAREMODE_SHARED,
		AUDCLNT_STREAMFLAGS_LOOPBACK,
		0, 0, fileFormat, nullptr
	);
	audioClient->GetService(__uuidof(IAudioCaptureClient), (void**)&captureClient);

	// Placeholder header
	WriteWavHeader();
}

void Audio::Start() {
	audioClient->Start();
	std::thread watcher([this]() { this->StdinWatcher(); }); // Watches for stdin -> stops recording

	while (!stopRecording) {
		UINT32 packetLength = 0;
		captureClient->GetNextPacketSize(&packetLength);

		while (packetLength != 0) {
			BYTE* data;
			UINT32 numFrames;
			DWORD flags;

			captureClient->GetBuffer(&data, &numFrames, &flags, nullptr, nullptr);
			std::vector<int16_t> buffer(numFrames * fileFormat->nChannels);
			float* fileData = reinterpret_cast<float*>(data);

			for (UINT32 i = 0; i < numFrames * fileFormat->nChannels; i++) {
				float sample = fileData[i];
				if (sample > 1.0f) sample = 1.0f;
				if (sample < -1.0f) sample = -1.0f;
				buffer[i] = static_cast<int16_t>(sample * 32767.0f);
			}

			audioFile.write(
				reinterpret_cast<char*>(buffer.data()),
				buffer.size() * sizeof(int16_t)
			);

			dataSize += buffer.size() * sizeof(int16_t);
			captureClient->ReleaseBuffer(numFrames);
			captureClient->GetNextPacketSize(&packetLength);
		}

		Sleep(10);
	}
}

void Audio::Stop() {
	stopRecording = true;

	audioClient->Stop();

	// Finalise wave header data size
	WriteWavHeader();
	audioFile.close();

	captureClient->Release();
	audioClient->Release();
	device->Release();
	enumerator->Release();
	CoUninitialize();
}

void Audio::WriteWavHeader() {
	DWORD byteRate = fileFormat->nSamplesPerSec
		* fileFormat->nChannels * 2;
	DWORD chunkSize = 36 + dataSize;
	DWORD subChunkSize = 16;
	WORD blockAlign = fileFormat->nChannels * 2;
	WORD audioFormat = 1; // PCM
	WORD bitsPerSample = 16;
	WORD channels = fileFormat->nChannels;
	DWORD samplesPerSec = fileFormat->nSamplesPerSec;

	audioFile.seekp(0, std::ios::beg);
	audioFile.write("RIFF", 4);
	audioFile.write(reinterpret_cast<const char*>(&chunkSize), 4);
	audioFile.write("WAVE", 4);
	audioFile.write("fmt ", 4);
	audioFile.write(reinterpret_cast<const char*>(&subChunkSize), 4);
	audioFile.write(reinterpret_cast<const char*>(&audioFormat), 2);
	audioFile.write(reinterpret_cast<const char*>(&channels), 2);
	audioFile.write(reinterpret_cast<const char*>(&samplesPerSec), 4);
	audioFile.write(reinterpret_cast<const char*>(&byteRate), 4);
	audioFile.write(reinterpret_cast<const char*>(&blockAlign), 2);
	audioFile.write(reinterpret_cast<const char*>(&bitsPerSample), 2);
	audioFile.write("data", 4);
	audioFile.write(reinterpret_cast<const char*>(&dataSize), 4);
}

void Audio::StdinWatcher() {
	// Watches for an stdin to stop recording
	std::string line;
	std::getline(std::cin, line);
	Stop();
}