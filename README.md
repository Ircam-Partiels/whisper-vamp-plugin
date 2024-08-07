# Whisper Vamp Plugin

The Whisper plug-in is an implementation of the [Whisper](https://github.com/openai/whisper) speech recognition model developed by [OpenAI](https://openai.com/) as a [Vamp plug-in](https://www.vamp-plugins.org/). The Whisper plug-in analyses the text in the audio stream and generates markers corresponding to the tokens (words and/or syllables) found.

The Whisper Vamp Plugin has been designed for use in the [Partiels](https://forum.ircam.fr/projects/detail/partiels/) application and requires the [Ircam Vamp Extension](https://github.com/Ircam-Partiels/ircam-vamp-extension). 

<p align="center">
<img src="./resource/Screenshot.png" alt="Screenshot" width=720>
</p>

## Compilation

The compilation system is based on [CMake](https://cmake.org/), for example:
```
cmake . -B build -G Ninja -DCMAKE_BUILD_TYPE=Release
cmake --build build
ctest -C Debug -VV --test-dir build
```

## Credits

- **[Whisper Vamp plug-in](https://www.ircam.fr/)** by Pierre Guillot at IRCAM IMR Department
- **[Whisper.cpp](https://github.com/ggerganov/whisper.cpp)** by Georgi Gerganov
- **[Whisper](https://github.com/openai/whisper)** model by OpenAI
- **[Vamp SDK](https://github.com/vamp-plugins/vamp-plugin-sdk)** by Chris Cannam, copyright (c) 2005-2024 Chris Cannam and Centre for Digital Music, Queen Mary, University of London.
- **[Ircam Vamp Extension](https://github.com/Ircam-Partiels/ircam-vamp-extension)** by Pierre Guillot at [IRCAM IMR department](https://www.ircam.fr/).  
