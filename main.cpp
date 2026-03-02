#include "Audio.h"

struct Settings {
    std::string filepath = "output.mp3";
};

Settings parseArgs(int argc, char* argv[]) {
    Settings settings;

    for (int i = 1; i < argc; ++i) {
        std::string arg = argv[i];

        if (arg == "--output" || arg == "-o") {
            if (i + 1 >= argc) {
                throw std::runtime_error("Missing value for --output / -o");
            }
            settings.filepath = argv[++i];
        }
    }

    return settings;
}

int main(int argc, char* argv[]) {
    CoInitialize(NULL);
    Settings settings = parseArgs(argc, argv);

    Audio audio(settings.filepath);
    audio.Start();

    CoUninitialize();

    return 0;
}