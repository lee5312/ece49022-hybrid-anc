# Plan A_7_6 - Live Visual Tuning

This plan starts from `plan_a_7_5_reference_spectral_suppressor`, but keeps
A_7_5 unchanged. A_7_6 adds two debugging/tuning features:

- stream the actual GP10 output sample as `$OUT,...` lines over USB serial
- change the suppressor parameters live over serial, without reflashing

The Proton still auto-starts standalone cancellation on power-up and still
outputs audio on GP10.

## What Is Included

This folder is intended to be self-contained for another computer:

- `src/main.cpp`: Proton firmware
- `platformio.ini`: PlatformIO build/upload config
- `boards/proton.json`: local Proton board definition
- `enable_usb_cdc.py`: enables USB serial logging/commands
- `post_upload_reset.py`: resets the board after upload
- `tools/live_visual_tuner.py`: optional PC visualization/tuning GUI

You still need PlatformIO, Python, the Proton toolchain packages that PlatformIO
downloads, and the hardware/debug probe for flashing.

## Build And Upload

From this folder:

```powershell
pio run
pio run -t upload
```

To open the live visualizer after upload:

```powershell
python .\tools\live_visual_tuner.py --port COM4
```

## Wiring

Two ICS43434 microphones share the same I2S clock and data pins. The `SEL` pins
choose which mic drives which I2S slot.

```text
Mic 1 3V       -> Proton 3V3
Mic 1 GND      -> Proton GND
Mic 1 BCLK     -> Proton GP0
Mic 1 LRCL/WS  -> Proton GP1
Mic 1 DOUT     -> Proton GP2
Mic 1 SEL      -> GND

Mic 2 3V       -> Proton 3V3
Mic 2 GND      -> Proton GND
Mic 2 BCLK     -> Proton GP0
Mic 2 LRCL/WS  -> Proton GP1
Mic 2 DOUT     -> Proton GP2
Mic 2 SEL      -> 3V3

Speaker/amp input -> Proton GP10
Speaker/amp GND   -> Proton GND
```

If the shared `DOUT` line floats when neither mic is driving it, add a `100 kOhm`
pull-down from `DOUT` to `GND`.

`GP10` is PWM audio output, not a speaker power driver. Use an amplifier input
or an active speaker input, with common ground.

## Standalone Behavior

Yes: after this firmware is uploaded, the Proton runs by itself.

On power-up:

1. Proton starts the ICS43434 I2S capture.
2. It starts the A_7_6 reference spectral suppressor.
3. It outputs the processed audio on `GP10` PWM.

The laptop is not in the audio loop. The PC GUI is only for live visualization
and tuning. If the GUI is closed or the USB cable is only supplying power, the
speaker path still runs.

Important: live slider changes are RAM-only. After a power cycle, the firmware
returns to the compiled default values listed below. Once a good setting is
found, bake those values into `src/main.cpp` so power-up starts with them.

## Live Visualizer

```powershell
python .\tools\live_visual_tuner.py --port COM4
```

The GUI shows:

- top panel blue: Mic1 raw PCM
- top panel orange: Mic2 raw PCM
- top panel thin darker lines: Mic1/Mic2 after the 80 Hz input high-pass
- bottom panel green: final GP10 output sample, just before PWM conversion
- env meters for output, Mic1, and Mic2
- sliders for live parameters

The GUI sends `speaker viz on 16` at startup and `speaker viz off` when closed.

The display zoom sliders only change the graph. They do not change the sound.

## Live Commands

```text
speaker status
speaker params
speaker viz on 16
speaker viz off
speaker gain 8
speaker mu 0.60
speaker refloor 8
speaker eps 20
speaker gate 0.02
speaker start 0.03
speaker strength 5
speaker min 0.06
speaker release 0.06
speaker reset
```

Changing these does not require upload. `speaker reset` only resets the current
mask/counters; it does not restore default slider values.

## Default Audio Parameters

```text
gain     = 8.0
mu       = 0.60
refloor  = 8.0
eps      = 20.0
gate     = 0.02
start    = 0.03
strength = 5.0
min      = 0.06
release  = 0.06
```

## Parameter Guide

The algorithm looks at each FFT frequency bin. If Mic2 has enough energy in a
bin, and that energy looks relevant compared with Mic1, the firmware lowers a
mask for that bin in Mic1. The final output is roughly:

```text
output = Mic1_bin * mask
```

`mask = 1.0` means pass normally. A smaller mask means suppress that frequency.

### Display Only

- `Raw display zoom`: visual scale for the Mic1/Mic2 raw graphs only.
- `Output display zoom`: visual scale for the GP10 output graph only.

These do not affect audio.

### Output Level

- `gain`

Final speaker volume after suppression and filtering. Higher is louder, but too
high can clip and sound harsh. If cancellation is working but quiet, raise this.
If it crackles or flattens visually, lower it.

### Suppression Speed

- `mu`

How fast the mask moves toward suppression when Mic2 hears a matching component.
Higher values react faster and make Mic2 movement more obvious. Lower values are
smoother and less likely to pump.

Good range to try: `0.30` to `0.80`.

### Mic2 Activity Threshold

- `refloor`

Mic2 must exceed this per-bin floor before the firmware trusts it as a reference
noise source.

Lower `refloor`:

- catches quieter Mic2 noise
- suppresses more often
- can react to Mic2 background hiss or handling noise

Higher `refloor`:

- ignores weak Mic2 signal
- preserves more Mic1 sound
- may miss quieter tool noise

### Quiet-Bin Stabilizer

- `eps`

This prevents the ratio `Mic2_power / Mic1_power` from exploding when Mic1 is
very quiet. It is a stability knob.

Lower `eps` makes the suppressor more aggressive in quiet Mic1 bins. Higher
`eps` makes it more conservative and smoother.

### Relevance Gate

- `gate`

Mic2 must be at least this relevant compared with Mic1 before suppression is
allowed.

Lower `gate` means "trust Mic2 more easily." Higher `gate` means "only suppress
when Mic2 is clearly meaningful."

### Suppression Start Point

- `start`

Suppression begins when this ratio is crossed:

```text
Mic2_power / Mic1_power > start
```

Lower `start` makes suppression begin earlier. Higher `start` waits until Mic2
is more dominant.

### Suppression Strength

- `strength`

How sharply the mask drops after `start` is crossed.

Higher `strength` removes Mic2-correlated sound harder. Too high can make the
audio hollow or watery because many frequency bins get carved out.

### Maximum Suppression

- `min`

The lowest mask value allowed. This is the hard floor for how much a frequency
can be reduced.

Examples:

```text
min = 0.20  -> keep at least 20% of that bin
min = 0.06  -> keep at least 6%
min = 0.02  -> very aggressive, almost remove the bin
```

Lower `min` means stronger cancellation but more risk of damaged audio.

### Release Speed

- `release`

How fast a suppressed bin returns to normal when Mic2 no longer hears that
component.

Higher `release` restores sound faster when Mic2 is moved away or covered. Lower
`release` makes suppression linger and can sound more stable, but it may keep
audio muted after the tool noise changes.

## Tuning Recipes

To make Mic2 movement more obvious:

```text
lower refloor
lower gate
lower start
raise strength
lower min
raise mu
```

To preserve speech better:

```text
raise refloor
raise gate
raise start
lower strength
raise min
lower mu
```

To reduce pumping:

```text
lower mu
lower release
raise eps
```

To recover faster after Mic2 is moved away:

```text
raise release
```

## Why Not Always More Aggressive?

More aggressive settings make the Mic2-near-noise test more obvious, but they
also increase the chance that speech or room sound in the same frequency bin is
removed. The risk points are:

- low `refloor`: random Mic2 energy triggers suppression
- low `gate` / `start`: Mic2 does not need to be dominant to suppress
- high `strength` / low `min`: audible spectral holes
- high `mu`: fast pumping when Mic2 moves
- low `release`: suppression stays after the noise source changes
