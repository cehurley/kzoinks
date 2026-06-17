<img width="200" height="200" alt="Copilot_20260616_220228" src="https://github.com/user-attachments/assets/bf1bc0b8-0e4b-4a00-bb09-19aeb356dce0" />
# KZoinks

A macOS standalone subtractive synthesizer built in C++ with JUCE. Features two detuned PolyBLEP sawtooth oscillators, a Chamberlin state-variable filter, amp and filter ADSRs, and full MPE support. Designed to be extensible via a drop-in module system.

## Features

- 2 detuned PolyBLEP saw oscillators
- Chamberlin SVF filter with ADSR envelope
- MPE support (lower zone, 15 note channels)
  - Pressure → filter cutoff
  - Timbre (CC74) → filter cutoff sweep
  - Pitch bend → pitch
- On-screen MIDI keyboard + physical MIDI/MPE input
- CoreAudio output (compatible with Loopback/JACK)

## Building

Requires macOS with Xcode command-line tools and CMake 3.22+.

```bash
cmake -B build -DCMAKE_BUILD_TYPE=Debug
cmake --build build -j$(sysctl -n hw.logicalcpu)
```

The app will be at `build/Synth_artefacts/Debug/Synth.app`.

For a release build:

```bash
cmake -B build -DCMAKE_BUILD_TYPE=Release
cmake --build build -j$(sysctl -n hw.logicalcpu)
```

## License

This project is licensed under the [GPL v3](LICENSE). It is built with [JUCE](https://juce.com), which is used here under the GPL v3 open-source license.
