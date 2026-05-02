# BCNmodular

VCV Rack plugins by BCNmodular (Barcelona, Catalonia, SPAIN).

By Santi Fort
---

## Maestro

**Probabilistic Voice Router, Arranger, Mixer & Sequencer**

Maestro is an arrangement tool for VCV Rack that brings controlled randomness to your patches. Instead of building fully deterministic sequences, Maestro acts as an intelligent conductor — deciding which voices play at each moment based on weighted probability, density control, and musical timing.

### The concept

In modular synthesis, achieving musical structure while preserving randomness typically requires combining many modules. Maestro consolidates this into a single module: connect your voices, set their relative probabilities, and let Maestro decide who plays at each evaluation point.

### Features

- **6 independent channels** — gate/clock or audio signals
- **Weighted probability per channel** — each channel has its own probability knob and CV input with attenuverter
- **Density control** — set how many voices are active at once, with CV modulation and attenuverter
- **Randomness control** — from fully deterministic (exact number of voices) to fully random (gaussian distribution)
- **Bar-based evaluation** — evaluations happen every N bars (1, 2, 4, 8, 16), not beats
- **Skip probability** — chance of keeping the current state instead of re-evaluating
- **Fade In / Fade Out** — per-channel switch between Gate mode (instant) and Fade mode (0–5s)
- **Polyphonic support** — all channels process polyphonic signals
- **Channel labels** — editable 4-character labels per channel (double-click to edit)
- **Active voices CV output** — CV proportional to the number of active voices
- **Bicolor LEDs** — green = open, yellow = fading, off = closed
- **Voltage indicator** — output jacks show signal level

### Context menu options

- **Beats per bar** — set time signature (2 to 8 beats per bar, default 4)
- **Min active voices** — set a minimum number of active voices to prevent full silence

### Controls

#### Global (Row 1)
| Control | Description |
|---------|-------------|
| CLOCK | Clock/trigger input — drives the beat counter |
| ACTIVE TRACKS | Number of channels participating in evaluation (1–6) |
| TRACK DENS | Base number of active voices (0–6) |
| DENS CV ATTVERT | Attenuverter for density CV (bidirectional) |
| DENS CV | CV input for density modulation |
| DETERM/RANDOM | Randomness amount (left = deterministic, right = random) |
| OUT CH. AMOUNT | CV output proportional to active voices (0–10V) |

#### Timing (Row 2)
| Control | Description |
|---------|-------------|
| LENGTH | Evaluation period in bars (1, 2, 4, 8, 16) |
| LEN CV | CV input for length (overrides knob) |
| SKIP PROB | Probability of skipping an evaluation (0 = never, 1 = always) |
| SKIP CV | CV input for skip probability (overrides knob) |
| FADE IN | Fade-in time for audio channels (0–5s) |
| FADE OUT | Fade-out time for audio channels (0–5s) |

#### Per channel (×6)
| Control | Description |
|---------|-------------|
| LABEL | Editable 4-character channel name (double-click) |
| INPUT | Signal input — gate, clock, or audio |
| PROB | Base probability for this channel |
| PROB CV ATTVERT | Attenuverter for probability CV |
| PROB CV | CV modulation for probability |
| GATE/FADE | Switch: Gate = instant on/off, Fade = smooth transition |
| LED | Status indicator: green = active, yellow = fading, off = inactive |
| OUT | Signal output |

### Typical use cases

**Arrangement tool** — Connect sequencers or voice outputs to Maestro's inputs. Use CLOCK from your master clock and set LENGTH from 1 to 16 bars. Maestro will periodically decide which voices are active, creating evolving arrangements that never repeat exactly.

**Performance tool** — Automate DENSITY with a slow LFO, any evolving signal generator, or MIDI CC to gradually bring voices in and out. Use the attenuverter to control how much the CV affects the density in real time.

**Gate sequencer** — Leave inputs unconnected (defaults to 1V) and use outputs to trigger gates, switches, or other modules. Maestro acts as a probabilistic relay.

**Mixer** — Connect audio signals and use Fade mode with longer fade times for smooth crossfades between voices. Without input signal, the output could be used also to control an external mixer or any voltage controlled module.

### Tips

- Set DETERM/RANDOM fully left for exact voice counts, fully right for maximum variation
- Use MIN ACTIVE VOICES in the context menu to avoid complete silence (if all voices are controlled by MAESTRO)
- In 3/4 time, set Beats per bar to 3 in the context menu
- Longer FADE OUT times preserve natural reverb tails when closing audio channels
- The ACTIVE CV output can feed back into DENSITY CV (with attenuverter) for self-regulating patches

### Building from source

```bash
# Clone the repository
git clone https://github.com/santifort-commits/BCNmodular.git
cd BCNmodular

# Set your Rack SDK path
export RACK_DIR=/path/to/Rack-SDK

# Build
make -j$(nproc)
```

### License

GPL-3.0-or-later — see [LICENSE](LICENSE) for details.

### Author

Santi Fort
BCNmodular — Barcelona, Catalonia  
https://github.com/santifort-commits/BCNmodular
