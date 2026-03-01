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


	// Initialise LAME
	lame = lame_init();
	lame_set_in_samplerate(lame, fileFormat->nSamplesPerSec);
	lame_set_num_channels(lame, fileFormat->nChannels);
	lame_set_VBR(lame, vbr_default);
	lame_init_params(lame);
}

void Audio::Start() {
	audioClient->Start();
	std::thread watcher([this]() { this->StdinWatcher(); }); // Watches for stdin -> stops recording

	std::vector<unsigned char> mp3Buffer(8192);
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

			// Encode PCM to MP3
			int bytesEncoded = 0;
			if (fileFormat->nChannels == 2) { // Dual channel
				bytesEncoded = lame_encode_buffer_interleaved(
					lame,
					buffer.data(),
					numFrames,
					mp3Buffer.data(),
					mp3Buffer.size()
				);
			} // Mono
			else {
				bytesEncoded = lame_encode_buffer(
					lame,
					buffer.data(), nullptr,
					numFrames,
					mp3Buffer.data(),
					mp3Buffer.size()
				);
			}

			if (bytesEncoded > 0) {
				audioFile.write(reinterpret_cast<char*>(mp3Buffer.data()), bytesEncoded);
			}

			captureClient->ReleaseBuffer(numFrames);
			captureClient->GetNextPacketSize(&packetLength);
		}

		Sleep(10);
	}

	// Flush LAME
	int flushBytes = lame_encode_flush(lame, mp3Buffer.data(), mp3Buffer.size());
	if (flushBytes > 0) {
		audioFile.write(reinterpret_cast<char*>(mp3Buffer.data()), flushBytes);
	}

	Stop();
}

void Audio::Stop() {
	audioClient->Stop();
	audioFile.close();

	captureClient->Release();
	audioClient->Release();
	device->Release();
	enumerator->Release();
	CoUninitialize();
}

void Audio::StdinWatcher() {
	// Watches for an stdin to stop recording
	std::string line;
	std::getline(std::cin, line);
	stopRecording = true;
}