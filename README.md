# Flang con Dulce de Leche

Plugin de audio (VST3, C++ / JUCE) inspirado en un amplificador clásico de
estado sólido: preamp limpio con EQ de 3 bandas, un drive/distorsión incorporado
y una sección de modulación conmutable **Chorus / Flanger** en true stereo.

> La referencia sonora está inspirada en un ampli de estado sólido famoso por su
> clean; el nombre y la identidad son originales.

Estado actual: **Fase 2 — preamp + EQ + drive**. Procesa audio: Input, Bass, Mid,
Treble, Drive (Type / Tone / Mix / Oversample) y Output. Todavía no hay modulación
(chorus/flanger).

## Requisitos

- macOS con Xcode (Command Line Tools o Xcode completo)
- CMake >= 3.22 y Ninja
- Internet la primera vez (FetchContent trae JUCE 9.0.3 y Catch2 3.9.1)

```sh
brew install cmake ninja
```

## Build

```sh
cmake -B build -G Ninja -DCMAKE_BUILD_TYPE=Release
cmake --build build
```

El plugin se copia solo a `~/Library/Audio/Plug-Ins/VST3/`. El standalone queda en
`build/Flang_artefacts/Release/Standalone/Flang con Dulce de Leche.app`.

> Apple Silicon: compila **arm64** a propósito. Live 11.1+ solo lista VST3 nativos
> arm64; un binario x86_64 no aparece y no da error.

## Tests (DSP offline, sin host)

```sh
cmake --build build
ctest --test-dir build --output-on-failure
```

Los tests viven en `Tests/` y ejercitan el núcleo puro (`Source/dsp/`) sin abrir
Live: procesan señales conocidas y verifican la respuesta.

## Estructura

| Path | Qué |
|---|---|
| `Source/dsp/` | Núcleo DSP puro (ToneStack, Preamp, Drive) — testeable sin host |
| `Source/PluginProcessor.*` | Adapter hacia el host (JUCE) |
| `Source/PluginEditor.*` | GUI |
| `Tests/` | Tests offline (Catch2) |

## Roadmap

| Fase | Qué |
|---|---|
| 0 | Scaffold + passthrough ✅ |
| 1 | Preamp + tone stack (EQ 3 bandas) ✅ |
| 2 | Drive (waveshaper soft/hard/fold + oversampling 2x/4x/8x + DC block) ✅ |
| 3 | ModEngine chorus (true-stereo) |
| 4 | Modo flanger |
| 5 | Cab IR (convolución) |
| 6 | GUI final, presets, pluginval |

## Licencia

JUCE se usa bajo su licencia dual (GPL / personal). Definir antes de distribuir.
