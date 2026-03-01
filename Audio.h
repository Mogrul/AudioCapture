#pragma once

#include <Windows.h>
#include <mmdeviceapi.h>
#include <Audioclient.h>
#include <iostream>
#include <atomic>
#include <functiondiscoverykeys_devpkey.h>
#include <thread>
#include <string>
#include <vector>
#include <fstream>
#include "lame.h"

class Audio
{
public:
	Audio(const std::string& fileName);
	~Audio() { Stop(); }

	void Start();
	void Stop();

private:
	void WriteWavHeader();
	void StdinWatcher();

private:
	IAudioCaptureClient* captureClient;
	IAudioClient* audioClient;
	IMMDevice* device;
	IMMDeviceEnumerator* enumerator;
	WAVEFORMATEX* fileFormat;

	lame_t lame;
	std::ofstream audioFile;
	DWORD dataSize = 0;
	std::atomic<bool> stopRecording = false;
};