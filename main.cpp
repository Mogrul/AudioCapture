#include "Audio.h"

int main() {
    CoInitialize(NULL);

    Audio audio("output.wav");
    audio.Start();

    CoUninitialize();

    return 0;
}