#include <Windows.h>
#include <mmdeviceapi.h>
#include <Audioclient.h>
#include <iostream>
#include <atomic>
#include <functiondiscoverykeys_devpkey.h>
#include <thread>
#include <string>

std::atomic<bool> stopRecording(false);

// Optional: catch Ctrl+C in console
void stdinWatcher() {
    std::string line;
    std::getline(std::cin, line);  // waits until Java writes something
    stopRecording = true;
}

int main() {
    CoInitialize(NULL);

    // Device enumerator
    IMMDeviceEnumerator* pEnumerator = nullptr;
    CoCreateInstance(__uuidof(MMDeviceEnumerator), nullptr, CLSCTX_ALL,
        __uuidof(IMMDeviceEnumerator), (void**)&pEnumerator);

    // Get default speaker
    IMMDevice* pDevice = nullptr;
    pEnumerator->GetDefaultAudioEndpoint(eRender, eConsole, &pDevice);

    // Display device name
    IPropertyStore* pProps = nullptr;
    pDevice->OpenPropertyStore(STGM_READ, &pProps);
    PROPVARIANT varName;
    PropVariantInit(&varName);
    pProps->GetValue(PKEY_Device_FriendlyName, &varName);
    std::wcout << L"Capturing from: " << varName.pwszVal << std::endl;

    // Activate audio client
    IAudioClient* pAudioClient = nullptr;
    pDevice->Activate(__uuidof(IAudioClient), CLSCTX_ALL, nullptr, (void**)&pAudioClient);

    // Get default format
    WAVEFORMATEX* pwfx = nullptr;
    pAudioClient->GetMixFormat(&pwfx);

    // Init loopback
    pAudioClient->Initialize(AUDCLNT_SHAREMODE_SHARED,
        AUDCLNT_STREAMFLAGS_LOOPBACK,
        0, 0, pwfx, nullptr);

    // Capture client
    IAudioCaptureClient* pCaptureClient = nullptr;
    pAudioClient->GetService(__uuidof(IAudioCaptureClient), (void**)&pCaptureClient);

    pAudioClient->Start();
    std::cerr << "Recording to stdout... send stop signal from Java to terminate.\n";

    // Thread to listen for a stop signal
    std::thread watcher(stdinWatcher);

    // Main loop
    while (!stopRecording) {
        UINT32 packetLength = 0;
        pCaptureClient->GetNextPacketSize(&packetLength);

        while (packetLength != 0) {
            BYTE* pData;
            UINT32 numFrames;
            DWORD flags;
            pCaptureClient->GetBuffer(&pData, &numFrames, &flags, nullptr, nullptr);

            // Convert float -> 16-bit PCM and write to stdout
            float* fData = reinterpret_cast<float*>(pData);
            for (UINT32 i = 0; i < numFrames * pwfx->nChannels; i++) {
                float sample = fData[i];
                if (sample > 1.0f) sample = 1.0f;
                if (sample < -1.0f) sample = -1.0f;
                int16_t intSample = static_cast<int16_t>(sample * 32767.0f);
                std::cout.write(reinterpret_cast<char*>(&intSample), sizeof(int16_t));
            }

            std::cout.flush(); // make sure Java sees it

            pCaptureClient->ReleaseBuffer(numFrames);
            pCaptureClient->GetNextPacketSize(&packetLength);
        }

        Sleep(10);
    }

    // Cleanup
    pAudioClient->Stop();
    pCaptureClient->Release();
    pAudioClient->Release();
    pDevice->Release();
    pEnumerator->Release();
    CoUninitialize();
}