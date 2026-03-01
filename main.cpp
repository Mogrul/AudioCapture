#include "Audio.h"

int main() {
    CoInitialize(NULL);

    Audio audio("output.mp3");
    audio.Start();

    CoUninitialize();

    return 0;
}