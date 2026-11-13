# DSP helpers and named profiling

Include `<resona/module.h>` and retain `resona_host_tools(host)` in your state
during init. A null result uses portable helpers and disables named hooks. Keep
that pointer across reset; it belongs to the instance and expires on unload.
These tools are for algorithm callbacks. Box App platform tables are unchanged.

```cpp
resona_profile_begin(state->tools, 0, "filter-bank");
resona_dsp_ring_q15(state->tools, rings, voice_count, output, frames);
resona_profile_end(state->tools, 0);
```

Available kernels:

| Helper | Contract |
| --- | --- |
| `resona_dsp_dot_q15` | Exact signed16×16 products accumulated in64 bits,1..4096 elements. Output must be8-byte aligned on device. |
| `resona_dsp_ring_q15` | Up to8 two-tap feedback rings,1..256 frames. Frame-major signed16 output. Each ring length1..16384 and index in range; coefficient0..32767. The result is floor(coefficient×(current+previous)/65536), then the original current becomes previous. Index advances with wrap. |
| `resona_dsp_f32_to_q15` |1..256 floats to signed16: NaN→0, clamp to[-1,32767/32768], scale32768, truncate toward zero. |
| `resona_dsp_q15_to_f32` |1..256 signed16 samples divided by32768 exactly. |

Pass null as the tools argument to select the portable reference explicitly.
Otherwise the optional protected backend is used when its capability is present.
Inputs must have their natural element alignment. Output cannot overlap inputs,
except that feedback rings are intentionally updated in place. Independent ring
buffers must not alias each other. The device validates every entire span against
that algorithm's allowed memory and bounds every job. Invalid native requests
fault the candidate/callback; the runtime never maps private firmware, models or
other instances to satisfy a request. Keep init and process bounded. Service time
counts toward the callback deadline and the existing CPU reservation.

## Measure before choosing

A native service call has a protection-boundary cost. Large dot products and
batched feedback rings can benefit; short conversion loops can be faster in
portable user code. Compare the same workload, state and memory placement, with
profiling consistently enabled or disabled. Device qualification tests the actual
selected placement and does not relax the chain's80% admission budget.

Names use nonempty ASCII up to23 bytes, with IDs0..7. Balance begin/end in the same
process call. Regions may overlap/nest with distinct IDs; their times overlap and
must not be added as exclusive CPU costs. They report elapsed wall time, including
gateway and interrupt/preemption overhead. First measure an empty scope if its
cost matters. These are not instruction-cycle counters or worst-case proofs.

In Creator Lab open **Device**, select the algorithm and choose **Start profiling**.
The list shows count, mean and peak time in microseconds. Stop profiling before
comparing normal CPU usage. Disabled hooks skip the gateway. Reset clears the
counters; reloading also clears them. Closing Device stops polling but leaves an
explicitly enabled measurement active until stopped or the instance is unloaded.

The same operations are available to scripts:

```sh
resona-lab profile --block 1 --action enable --json
resona-lab profile --block 1 --action read --json
resona-lab profile --block 1 --action reset --json
resona-lab profile --block 1 --action disable --json
```

The shared C API operation is `dsp_profile`, with `block`1..5 and an `action`
string from that list. Requires firmware supporting the tools/profile extension.
Older memory-aware firmware uses portable DSP and has no device profile command.
Desktop previews run the portable semantics; they do not claim device timing.
The [DSP Tools example](../../examples/dsp_tools/README.md) checks equivalence and
profiles a4096-element dot product. Native implementation details remain in the
platform firmware, outside this standalone source distribution.
