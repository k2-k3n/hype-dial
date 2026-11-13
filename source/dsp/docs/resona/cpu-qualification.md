# CPU qualification

A quiet synth or an empty delay buffer can hide the expensive state of an algorithm.
The previous eight-block target trial covered about 10.7 ms at 64 frames, missing
the later 100 ms granular-freeze workload observed on the bench.

## Target policy

The candidate runs separately from the live slot, at each of 64/128/256 frames:

- Defaults: 250 ms silence followed by 1000 ms of deterministic excitation.
- All controls at maximum and midpoint: 500 ms each. All minimum and two
  complementary minimum/maximum patterns: 250 ms each.
- For each control: its minimum and midpoint with all other controls at maximum
  (125 ms each), and its maximum with other controls at defaults (64 ms).
- If supplied, a module-specific active/transition/release sequence: 500 ms per
  phase. Release gets silent input. History persists between those phases.

Signal history persists across the generic parameter changes, so tests can fill
buffers and exercise middle modes with maximum buffer lengths. Input includes
silence, DC, alternating signs and deterministic broadband excitation. Values
are quantized through the declared parameter metadata. UI/USB scheduling yields
occur between batches of eight probes; they are outside measured processing.
Live audio callbacks stay at their existing priority and their completed time
is excluded from the candidate measurement. Processing that already exceeds
the admission ceiling stops further probes.

Reservation is the worst measured per-64-frame cost plus 50% and eight ticks,
with a 1% floor, or the declared minimum if larger. Combined admission remains
80% including the platform allowance. The installed instance starts freshly
initialized at defaults, without any test notes or buffer history. Longer tests
can make imports and preset recalls take longer; only their request deadlines
are extended. Other request timeouts stay unchanged.

## Optional SDK profile

Append a pointer to `resona_cpu_profile` in `resona_module.cpu_profile`:

```cpp
void prepare_trial(void *state, uint32_t phase) {
    // ACTIVE: start the maximum supported chord.
    // TRANSITION: change modes while that chord is sounding.
    // RELEASE: release the voices and exercise the tail.
}
const resona_cpu_profile cpu = {
    RESONA_CPU_PROFILE_VERSION, sizeof(resona_cpu_profile),
    250, // illustrative 25% minimum; choose using measured device evidence
    prepare_trial
};
```

`min_permille` is 0..800; 0 selects automatic reservation. A declaration can only
raise the reservation, never lower the measured result. The callback is optional,
so a profile can supply only a reservation floor. It follows the control-thread
rules: bounded, no I/O, no allocation and no waiting. Its setup time is not audio
CPU; subsequent process calls are measured. It must prepare representative work
without suppressing normal DSP. Lab check also exercises these phases for finite
output; its desktop timings do not qualify hardware CPU.

The SDK's `examples/cpu_workload` demonstrates the API with a subtractive synth. Normal
preview stays silent. A polyphonic synth must start its maximum supported voices
in the hook; generic parameter probing cannot discover every note sequence.

The profile uses an optional ABI1 tail (76-byte RV32 module, 16-byte profile).
The runtime supports 68/72-byte layouts within packages using the matching target;
see [package target](resona-box.md#package-target). Newly rebuilt 76-byte packs
require the matching 2026-10-20 firmware update, even if the profile is null.
The updater does not resize or rewrite any preset or pool storage.

## What Lab reports

Current CPU, raw tested peak and reserved capacity are distinct. The raw tested
peak excludes the margin and developer floor. Runtime peak includes later live
processing. A bypass message compares that algorithm's runtime peak with its own
reservation; it does not imply that the whole device used 100% CPU. Resource
report v3 adds the tested peak; older v2 firmware reports it as unavailable.

These probes are representative measurements, not exhaustive worst-case proofs.
A long delay or an untested note/parameter combination may still cost more later.
The protected runtime interrupts stuck user callbacks and contains access faults.
Measured reservations still cannot prove arbitrary worst-case time.

## Memory capacity

Use the live fast RAM, normal RAM and code budgets in Lab's Device page or
`chain status`. Platform code, factory processing, stacks and buffers reserve part
of the installed memory. Heap totals describe currently managed memory, not the
whole hardware capacity. State and explicit buffers share live SRAM/PSRAM admission; algorithm packs
do not yet have a public normal-RAM allocator.

## Verification status

Physical workload/floor/freeze tests were performed on Resona Box on2026-10-20;
this SDK source tree does not contain their device captures. This guide describes
the runtime contract, not a standalone reproduction of those measurements.
Representative measurements do not establish arbitrary code's worst case or full
product qualification. Retest your own maximum workload on compatible firmware.
