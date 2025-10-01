# Granular Pitch Shifter

A real-time granular pitch shifter CLAP plugin that turns recent audio into a playable instrument with intelligent harmonic generation. Very early in development right now (it's rough).

## Features

- **Real-time Granular Synthesis**: Captures and manipulates audio in real-time using grains
- **Intelligent Onset Detection**: Automatically triggers grains based on audio energy and transients
- **Harmonic Generation**: Multiple harmonic modes for musical grain clusters
- **Parameter Smoothing**: Anti-zipper noise smoothing system with denormal protection
- **Safety Clipper**: Prevents digital clipping with soft limiting
- **Cross-Platform Build**: Automated builds for Linux, macOS (Intel/ARM), Windows
- **Clean Architecture**: Separated DSP, parameters, and utility code

## Parameters

| Parameter | Range | Description |
|-----------|--------|-------------|
| **Grain Size** | 10-200ms | Length of individual grains |
| **Trigger Sensitivity** | 0-100% | Onset detection sensitivity threshold |
| **Pitch Spread** | 0-24 semitones | Random pitch variation range |
| **Grain Density** | 1-8 grains | Number of grains per trigger |
| **Auto Harmony** | Off/Octaves/Fifths/Triads/Pentatonic | Harmonic mode selection |
| **Dry/Wet Mix** | 0-100% | Mix between processed and dry signal |

## How It Works

The plugin continuously records incoming audio into a circular buffer (2 seconds). When onsets are detected:

1. **Onset Detection**: Energy-based analysis triggers grain clusters
2. **Harmonic Generation**: Creates multiple grains with musical intervals
3. **Consonance Filtering**: Removes dissonant intervals for musical results
4. **Grain Processing**: Each grain plays back with pitch shifting and envelope shaping
5. **Safety Processing**: Soft limiting prevents clipping

## Harmonic Modes

- **Off**: Single grain at original pitch
- **Octaves**: Octave-based harmonies (12, 24 semitones)
- **Fifths**: Perfect fifth intervals (7 semitones)
- **Triads**: Major triad harmonies (root, 3rd, 5th)
- **Pentatonic**: Pentatonic scale intervals

## Building

### Local Build
```bash
cmake -B build -DCMAKE_BUILD_TYPE=Release
cmake --build build --config Release
```

### Automated GitHub Builds
Push a tag to trigger automatic builds for all platforms:
```bash
git tag v1.0.0
git push origin v1.0.0
```

## Installation

Copy the `.clap` file to your plugin directory:
- **Linux**: `~/.clap`
- **macOS**: `~/Library/Audio/Plug-Ins/CLAP`
- **Windows**: `%COMMONPROGRAMFILES%\CLAP`

## Technical Details

### Architecture
- **plugin.h**: Main header with constants and structures
- **plugin.cpp**: CLAP interface implementation and plugin lifecycle
- **dsp.cpp**: Digital signal processing and parameter smoothing
- **params.cpp**: Parameter management and state saving/loading
- **utils.cpp**: Utility functions for audio processing and grain management

### Key Features
- **Cross-platform SIMD**: Denormal protection with flush-to-zero
- **Efficient Processing**: Only updates coefficients when parameters change
- **Memory Management**: Dynamic buffer allocation based on sample rate
- **State Persistence**: Complete save/restore with version checking
- **Grain Voice Pool**: Efficient allocation and voice stealing

### Performance Optimizations
- Exponential parameter smoothing (5ms)
- Lazy coefficient updates
- Soft limiting instead of hard clipping
- Efficient circular buffer operations
- Voice stealing for grain management

## License

Public domain template - use however you want.

## Credits

All Coding by Claude AI
Granular synthesis implementation with CLAP plugin architecture and automated build system.
