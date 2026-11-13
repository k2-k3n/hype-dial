# The Resona Box

A guitar amp-and-effects box with a protected Resona audio runtime, a320 ×172
colour screen, two push-encoders, a status light, USB audio/MIDI and wireless audio/MIDI.
Write against the Resona APIs; platform internals and peripheral drivers are private.

| | |
| --- | --- |
| Audio | 48 kHz, mono guitar input, stereo output |
| Block size | 64 (default), 128 or 256 frames: 1.33 / 2.67 / 5.33 ms. Larger blocks give each pack call more time and add latency (+2.7 ms at 128, +8 ms at 256, round trip). Set in the factory app's Settings (BLOCK SIZE) or by an app |
| Chain | five block positions: GATE, COMP, RESONA (amp profile), NAM, CAB (impulse response). Each runs its factory engine or an algorithm pack; the order can change |
| Pack CPU limit | Device measures stateful/combined-setting trials, reserves the worst normalized cost plus50% and8 ticks (minimum1%), or a larger declared floor. All resident packs plus platform allowance must fit80% total. Custom packs bypass on their own first reservation overrun. See [CPU qualification](cpu-qualification.md) |
| Pack memory | Dedicated128 KiB fast code pool by default, or explicit code in shared SRAM/PSRAM; state and buffers placed in fast SRAM or PSRAM, admitted together against live free heaps. See [DSP memory](dsp-memory.md). Runtime stacks/buffers and factory work also consume memory. Read live free/used budgets from Device; physical capacities do not equal allocatable memory |
| Screen | 320 × 172, RGB565 |
| Encoders | SELECTOR (Encoder 1, top) and CUSTOMIZE (Encoder 2, bottom): turn, click, long press, double press |
| Flash | half for the platform and the box app (app image up to 192 KiB), half for user assets: at least 30 amp profiles (EAD), 30 NAM models and 30 impulse responses, plus a shared pool for packs |
| Presets | 50 slots |

## Two kinds of user code

| | Algorithm pack | Box app |
| --- | --- | --- |
| What | sound processing in one block position | the whole UI and operation logic |
| API | `resona/module.h` | `resona/platform.h` |
| How many | one per position, several at once | one per box |
| Install | `resona-lab pack try` / `pack install` | `resona-lab app install` |
| Runs | in the audio thread, every block | on the UI thread, ~30 frames a second |
| Rules | strict real-time (see packs.md) | must not block its thread (see box-apps.md) |

Packs are built for a family of Resona devices, not only this box: they take the sample rate,
block size and channel count from the host, so the same pack can run on later devices.

## Talking to a box

### Package target

Algorithm and Box App packs use the Resona-specific `RBX1` target. SDK, builder
and installed firmware must match. Older target IDs have no alias: rebuild old
packs and preset pack dependencies from source before using the updated runtime.
New packs require the matching firmware update. Updating headers does not flash,
migrate or erase any device storage.

`resona-lab` finds a box connected over USB (USB-MIDI). For the device simulator set
`RESONA_SIM_MIDI` to the simulator's QMP socket. A box needs the Resona platform firmware
(with the app loader and pack pool) to accept packs and apps.


On resource8 firmware, Custom slots may repeat protected factory engines with
independent controls/models. Five slots and shared CPU/memory admission decide
fit. Native CPU is included in whole-chain cost, not a user-pack reservation.
Do not infer that two loaded engines can both be enabled. Use factory_model_select
for per-slot library models, or Box App block_model_select. SDK dev.5 mirrors
independent controls in local UI preview; it does not execute factory DSP.


Resource9/dev.6: factory NAM has a fixed maximum of two resident slots (bypassed
also count). With both active,80% becomes a warning and100% is the audio deadline
limit; ordinary chains remain80%. Shared resource results include dual_nam,
dual_nam_active, cpu_warning_ticks and cpu_warning. Read actual usage; added
USB/Bluetooth work may trigger overload recovery. Three consecutive or8/64
deadline misses bypass the last NAM. User-pack isolation/budgets and all memory
checks remain; do not force a third native NAM or promise every workload fits.
