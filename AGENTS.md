# AGENTS.md

Reglas para agentes de codigo (y humanos) en este repo. Es **configuracion ejecutable**, no onboarding.

## Project

`Flang con Dulce de Leche` — plugin de audio (C++ / JUCE) de la org **MallinLabs**: preamp limpio + EQ, drive incorporado, y modulacion conmutable Chorus/Flanger true-stereo.

## Estructura

| Path | Que |
|---|---|
| `Source/` | Codigo del plugin (adapter hacia el host) |
| `Source/dsp/` | Nucleo DSP puro — testeable sin host |
| `Tests/` | Tests offline del DSP (Catch2) |
| `Assets/ir/` | Impulse responses de parlante (Fase 5) |

## Reglas MUST

- **Real-time safety**: en `processBlock` NO hay allocs, locks ni I/O. Todo se reserva en `prepareToPlay`.
- **El DSP no sabe nada del host**: la logica de audio vive en `Source/dsp/`, pura y testeable sin JUCE host.
- **TDD**: test que falla primero, despues la implementacion (aplica a `Source/dsp/`).
- **Smoothing** en todo parametro continuo (`SmoothedValue`) — nada de cambios crudos.
- **Reportar latencia** con `setLatencySamples()` cuando una etapa la introduzca (oversampling, convolucion).
- **Conventional Commits**: `type(scope): subject`. Nunca agregar atribucion de IA ni `Co-Authored-By`.
- **Blocks variables**: el plugin debe sobrevivir a bloque 0, mono, estereo y sample rates raros.

## No hacer

- No romper IDs de parametros (rompe automatizacion guardada).
- No meter dependencias del host dentro de `Source/dsp/`.
- No commitear `build/` ni artefactos de plugin.
