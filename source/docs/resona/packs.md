# Writing an algorithm pack (a Resona module)


CPU-profile firmware adds an optional `cpu_profile` module tail: representative
active/transition/release workloads and a minimum reservation that can only
increase measured cost. See [CPU qualification](cpu-qualification.md).

For pack authors and the AI agents they work with. Every new project has this file in
`docs/resona/`; `AGENTS.md` points here.

## What you are building

One C++ translation unit (or a few) that exports `resona_module_get()` and fills in
a `resona_module` (see `include/resona/module.h`): metadata, up to 12 parameters,
a state size, and four functions: `init`, `reset`, `set_param`, `process`.

The audio contract: **48 kHz, mono float in, mono float out, in place.** `process()` gets
1 to `host->max_block_frames` frames (256 on the Resona Box); the box runs blocks of 64,
128 or 256 frames as its owner sets, so never assume 64. On the Resona Box a pack sits in
one of the five block positions of the chain (Gate, Comp, Resona, NAM, Cab), in place of
that position's factory engine; several packs can run at once, each in its own position.

## The loop

```sh
resona-lab new  ~/Documents/"Resona Lab"/my-fx --name "My FX"
resona-lab build  my-fx          # compile + gate 1 (forbidden imports)
resona-lab render my-fx --param 0=12          # listen to build/render.wav
resona-lab check  my-fx          # gates 1 and 2 stress checks
```

`check` must pass before you install anything on a device.

## Rules for `process()` (it runs every 1.33 ms at 64 frames)

1. **No heap.** No `malloc`, `free`, `new`, `delete`, `std::vector`, `std::string`.
   Keep small state in `State`; declare large/placed buffers with `memory_profile`.
   The host allocates before init; see [DSP memory](dsp-memory.md).
2. **No exceptions or RTTI.** The build uses `-fno-exceptions -fno-rtti`.
3. **No C library.** The device build links none, so `<math.h>`, `<stdio.h>` and the
   rest are not available (64-bit integer division and int64 ↔ float conversions are fine:
   the SDK runtime provides them). Use `<resona/math.h>` (`resona_db_to_lin`, `resona_expf`,
   `resona_powf`, `resona_sinf`, `resona_tanhf`, ...), which is identical on the
   desktop and the device. Compute coefficients in `set_param()` / `init()`, not in
   `process()`; a build that references a missing symbol fails at link time.
4. **No I/O.** No `printf`, files or sockets. `host->log` works in `init()` only.
5. **Bounded work.** Every loop has a fixed upper bound. No waiting, no locks.
6. **Never emit NaN or infinity.** Guard divisions, clamp feedback, and flush values
   below ~1e-20 to zero inside feedback paths (see the Delay example).
7. **`reset()` clears all signal history.** After `reset()` the same input must give
   the same output; `check` verifies this.
8. **Clamp nothing you do not own.** The host already clamps parameter values to the
   range you declared.

## Building blocks

A block is a small DSP part: a struct with `init()`, `reset()`, setters in user units
(`set_rate(hz)`, `set_mix(percent)`) and `process(x)` per sample (or `process_cv(x, mod)` to
drive its modulation from another signal). The SDK has a list of them in
`sdk/include/resona/blocks/`: sources (`Lfo`, `Osc`, `Noise`, `Value`), control (`Scale`, `Slew`,
`SampleHold`), level (`Gain`, `Mixer`, `Multiply`), tone (`Filter`, `EqBand`, `OnePole`,
`PoleZero`, `PhaseStage`), time (`DelayLine<N>`, `Comb<N>`, `Allpass<N>`, `UnitDelay`), dynamics
(`Envelope`, `DynCurve`) and distortion (`Drive`, `Clip`, `Fold`, `Crush`, `Rectify`). They follow
the rules above: no heap, no libm, no NaN, and `reset()` clears their history.

**View blocks** (`Scope` "Waveform", `Spectrum`, `Meter` "Level") only look at a signal: they
have an input and no output, so they cannot change the sound (`d.scope1.process(x);`). In the
Lab's desktop build (`RESONA_LAB_HOST` is defined) they keep the last 2048 samples, which the
Flow page draws live while the project plays; on the device they compile to nothing.

**Use one in a project** with Add block in the app, or:

```sh
resona-lab block add delayline --project my-effect          # --name wob to name it
```

That copies `delayline.h` into the project's `blocks/` (so the project holds everything it builds
from, and the file is yours to edit), then edits your code:

| In | It adds |
| --- | --- |
| `src/dsp.h` | `#include "blocks/delayline.h"` and the member `resona::blocks::DelayLine<2400> delayline1;` in `struct Dsp` |
| `src/dsp.cpp` | `d.delayline1.init();` and its setters in `dsp_init`, `d.delayline1.reset();` in `dsp_reset`, and the call in `dsp_process`, above the line that writes `out[i]` |

It finds `struct Dsp`, `dsp_init`, `dsp_reset` and `dsp_process` by name, so it works on files
you have changed; anything it cannot place is reported (`notes`) instead of guessed. The call is
yours to wire:

```cpp
const float delayline1_out = d.delayline1.process(in[i]);   // process_cv(x, mod) takes a modulation signal
out[i] = in[i] + 0.4f * delayline1_out;
```

**The Flow** (`flow.json`) is the project's chain as a graph of blocks, kept apart from the
code. The Lab plays it through a desktop build of its own (`build/flow-engine/`), so a change is
heard at once without touching `src/`; **Sync** writes it into the code: `src/dsp.h`,
`src/dsp.cpp` and `src/params.h` become the graph's, and the block files are copied into
`blocks/`. It goes one way, Flow to code: a project's code is read into a graph once, when it
has no `flow.json` yet. A graph holds blocks only, so maths of your own goes into a block of
your own (a header in `blocks/`), which the Add menu then lists under This project.

```sh
resona-lab flow my-effect                        # the graph, and whether it is synced
resona-lab flow my-effect add delayline          # also: remove, connect, disconnect, set, knob
resona-lab flow my-effect connect input.out delayline1.in
resona-lab flow my-effect knob delayline1.time on   # a value as one of the module's knobs
resona-lab flow my-effect slot delayline1.time 5    # where it sits on the box (slot 1-12, 4 a page)
resona-lab flow my-effect label delayline1.time Echo   # its name there (12 characters)
resona-lab flow my-effect link comb1.gain,comb2.gain --label Feedback   # one knob, two values
resona-lab flow my-effect screen --event selector:click --png glass.png   # the box screen
resona-lab flow my-effect preset tape_echo       # the graph becomes a ready-made effect
resona-lab flow my-effect engine                 # build what the Flow plays (and report)
resona-lab flow my-effect sync                   # write the Flow into the code
resona-lab flow my-effect --code                 # what the code itself describes
```

A block that nothing wires to the Output is left out of the code (Sync names it). After a
Sync, editing `dsp.cpp` by hand is noticed ("code edited since the last sync"); the next Sync
replaces those edits. It reads the plain pattern above: a value
named after the block and port, passed on as an argument. Code that hides a block call in an
expression it cannot follow is left out of the diagram, and said so.

## Writing a block

A block is one C++ header. Open any block from its file in `blocks/` (or `resona-lab block show
<kind>`). The SDK's blocks are read-only until they are in your project; `resona-lab block new
my_kind --from delayline` makes a copy under a new kind, and **New block** starts from a
template. Your own blocks live in the project's `blocks/`, or in `Documents/Resona Lab/blocks/`
to share between projects; a project that uses one keeps a copy, so it builds anywhere.

A comment at the top of the file is the block's interface. Everything the app knows about the
block (its ports, its parameters, its options, how much state it needs) is in it:

```cpp
/*@block
 * kind      delayline            lower-case id; the file is <kind>.h
 * title     Delay Line
 * short     DLN                  1 to 4 characters: prefix of the module's parameter names
 * category  Time                 the palette group
 * legacy    no                   yes hides it from the palette (old patches still load)
 * summary   The signal a set time ago ...  (continue on lines indented deeper)
 *
 * in        in audio             a port: name, audio or cv, optional  label "text"
 * in        mod cv label "time" moves time by depth
 * out       out audio
 *
 * param     time "Time" TIME ms 0.02 500 20 expose set_time
 *           id   label  short unit min max default  expose|hidden  method
 * param     shape "Shape" SHAPE - 0 3 0 hidden set_shape choices "Sine" "Tri" "Saw" "Square"
 * param     depth "Mod depth" MOD ms 0 500 0 expose set_mod_depth
 * option    max "Longest, ms" default 50 values 10 50 250 500
 *
 * type      DelayLine<{max*48}>  the struct, with options in {braces} as arithmetic
 * state     4*max*48 + 32        bytes of State it needs (shared live memory admission; declarations must fit their selected memory region)
 * late      yes                  only for a block that is a one-sample delay (below)
 */
```

The struct under it needs `init()`, `reset()`, one setter per `param` (it receives the value in
the parameter's unit) and `process()`:

- a project's code (and the generated code of an older block canvas) calls `process()` with the
  signal inputs in port order;
- a `mod` port goes last, and a connected one switches the call to `process_cv(..., mod)`
  (write both; `process()` runs when nothing is cabled to it);
- a cv input may say what it moves, so the Flow shows that value riding: `moves <param> by
  <amount>` adds up to ± amount in the value's unit, `moves <param> octaves <amount>` multiplies
  by 2^(± amount); the amount is a number or another parameter (`by depth`), and `when <choice>
  <value>` limits it to one setting (`moves freq octaves sweep when control 0`). It describes
  what your `process_cv()` does; it does not change the generated code;
- a block with several outputs returns the first and takes the others as `float &` parameters
  after the inputs, in port order;
- a block with no input is called with no arguments;
- feedback: a cable may loop back only through a block marked `late yes`. It has `float out()
  const` (its input from the previous sample) and `void write(float x)` (called after the whole
  sample) instead of `process()`. The built-in Unit Delay is one; write your own to put state
  in a loop, such as a delay with a built-in filter.

No heap, no exceptions, no C library; include `"resona/blocks/common.h"` for `clampf`,
`flush`, `sin_unit`, `tan_hz` and the sample rate, and `<resona/math.h>` for `exp2`, `log2`,
`db_to_lin`. **Check** (or `resona-lab block check <kind>`) builds a throw-away
Input -> block -> Output module and puts it through the same finite/bounded/deterministic
stress checks as any module.

## Screens

A module may also draw its own screen on the Resona Box (`ui_draw` in `resona_module`;
leave it `NULL` for an audio-only module). The canvas is 320 x 172 pixels, RGB565. The
host's primitives, all in `<resona/module.h>` and wrapped in `<resona/ui.h>`:

| Call | Draws |
| --- | --- |
| `clear`, `rect` | a fill, a filled rectangle |
| `text` | the built-in 5x7 face, scale 1 to 8 |
| `line`, `dotline` | a 1 px line, a dotted one (the box's rule) |
| `ellipse`, `ellipse_fill` | an ellipse outline, a filled one |
| `dots` | square dots spaced along an elliptical arc (rings of LEDs) |
| `poly`, `poly_fill` | a closed polygon outline, a filled convex polygon |

How it runs: the host calls `ui_draw` about 20 times a second, **outside** the audio
thread, and records your calls into a bounded list (128 calls and 512 bytes of text per
frame; the rest are ignored and counted). It then executes the list. The device and
the desktop preview use the same recorder and executor, so what you see in the preview
is what the box shows. `resona-lab ui-verify` proves it: it loads your module, sets the
same parameters, captures the device screen and compares it with the preview pixel for
pixel. On a box whose firmware predates the shape calls, `<resona/ui.h>` falls back to
rectangles and `<resona/widgets.h>` to text, so a module still draws.

Rules for `ui_draw`: read the state, never write it; no heap, no loops without a
bound; it must return quickly. A `ui_draw` that does not return makes the device reset.

```sh
resona-lab ui-preview my-fx --param 0=12 --scale 3     # build/ui.png
resona-lab pack try   my-fx --block COMP                # run it on the box, from RAM
```

The Resona Lab app does the same in one window: edit, ⌘S to rebuild and preview,
sliders for the parameters, and the device panel's blocks. (`resona-lab load`, `ui-verify`,
`event` and `screenshot` talk to the older Lab firmware, not to a Resona Box running its
factory firmware.)

## The UI kit (`<resona/widgets.h>`)

Ready-made widgets in the box's own look (its palette and pixel font, drawn as thin
isometric line figures): `Knob`, `Fader`, `Meter`, `Toggle`, `Steps` and `XYPad`, and a
`Form` that holds them, draws the title, page counter and footer hint, moves a selection
with SELECTOR, edits the selected one with CUSTOMIZE, and says which widget changed.
They are plain classes with no heap and no virtual functions; put them in your `State` and
set each up in `init()` (the host gives a module zeroed memory, so there are no constructors):

```cpp
struct State { resona::ui::Knob cutoff, res; resona::ui::Fader gain; resona::ui::Form form; };

void init(void *s, const resona_host *) {
    State *st = static_cast<State *>(s);
    st->cutoff.init(46, 72, "CUTOFF", 20.0f, 12000.0f, 1000.0f, "HZ").log_scale();
    st->res.init(122, 72, "RES", 0.0f, 100.0f, 20.0f, "%");
    st->gain.init(200, 104, "GAIN", -24.0f, 12.0f, 0.0f, "DB").decimals(1);
    st->form.init("FILTER");
    st->form.add(&st->cutoff);          // each add() returns the widget's id
    st->form.add(&st->res);
    st->form.add(&st->gain);
}
void ui_draw(void *s, const resona_ui *ui) { static_cast<State *>(s)->form.draw(ui); }
void on_event(void *s, const resona_event *e) {
    State *st = static_cast<State *>(s);
    if (st->form.handle(*e) == 0) set_cutoff(st->cutoff.value());   // id 0 changed
}
// in process(): st->form.tick();   // eases the lift and the pointers, once a block
```

| Widget | `init(...)` | Notes |
| --- | --- | --- |
| `Knob` | `x, y, label, min, max, default, unit` | `.log_scale()`, `.step(s)`, `.decimals(n)`; about 13 calls |
| `Fader` | `x, y, label, min, max, default, unit, length` | a rail and a cube; about 15 calls |
| `Meter` | `x, y, label, plates` | feed with `set(0..1)` and `set_peak()`; a display only |
| `Toggle` | `x, y, label, on` | `on()`, `set()`, `press()` |
| `Steps` | `x, y, count, columns` | `mask`, `playhead`, `cursor`, `set_step()` |
| `XYPad` | `x, y, label, size` | `x_value()`, `y_value()`, `set(x, y)` |

Every one has `value()`, `set(v)`, `draw(ui, mode)` (mode: `Rest`, `Selected`, `Editing`),
`turn(detents)`, `press()` and `tick()`. Draw them yourself without a `Form` if you want a
different layout. `examples/widgets` shows every widget on three pages; a project made from a recipe
own screen is a `Form` of `Knob`s, four to a page.

The Effect template's `src/ui.cpp` puts knob `i` in the next place, four to a page, unless
params.h defines `P_SLOTS`: then knob `i` sits at `kSlot[i]` (index 0 to 11 = slot 1 to 12 as
the Lab and the command line show it; place `index % 4`, page `index / 4`), the knobs join the
`Form` in slot order, and `screen_event` maps the form's widget back to its parameter. A page
can then have no knob, and the `Form` keeps its selection on another page's knob there:
`screen_event` ignores CUSTOMIZE on such a page, so nothing unseen changes (do the same in a
`ui.cpp` of your own). A Flow writes `P_SLOTS` and `kSlot` only when a knob was given a slot or
a link exists. A Sync replaces a `ui.cpp` that is exactly an earlier template's (line ends and
a byte-order mark aside); a `ui.cpp` of your own stays, so read `kSlot` there yourself if you
want the slots.

## Input

The box has two push-encoders: **SELECTOR** (top) and **CUSTOMIZE** (bottom). A
module that wants them sets `on_event`:

```cpp
void on_event(void *s, const resona_event *e) {
    State *st = static_cast<State *>(s);
    if (e->input == RESONA_INPUT_SELECTOR && e->kind == RESONA_EVENT_CLICK)
        st->bypass = !st->bypass;
    if (e->input == RESONA_INPUT_CUSTOMIZE && e->kind == RESONA_EVENT_TURN)
        st->level += 0.5f * (float)e->value;  // + is clockwise
}
```

Events are gestures the box has already classified: `RESONA_EVENT_TURN` (value =
signed detents, quick turns merged), `RESONA_EVENT_CLICK` (on release) and
`RESONA_EVENT_LONG`. They arrive on the screen thread, in order, once per screen refresh;
`on_event` may run while `process()` runs, so write plain values the way `set_param()`
does, and keep it short: one that does not return takes the device down.

Try it without the box: `resona-lab ui-preview my-fx --event selector:click --event
customize:turn:+3` replays events before drawing (the same sequence the box would see),
`resona-lab event selector click` sends one to the box, and in the app the two big
controls on the drawn device respond to a click, a long press and the scroll wheel.

## State and parameters

- Everything that persists between blocks lives in `State`; there are no mutable
  globals. Static `const` tables are fine.
- `state_size` is a declaration, checked with all buffers against shared live memory.
  The optional `state_region` tail selects fast SRAM (zero/default) or PSRAM; keep hot state small
  and say what dominates it.
- Parameter names are at most 12 characters (the device display width). Give each a
  unit string and a default inside `[min, max]`.

## Things the desktop run cannot tell you

`render` and `check` run on your computer. They say nothing about CPU time, memory
placement or timing on the device (that is gate 3, measured on the Resona Box). A
module that passes here can still be too slow on the device; keep `process()` simple
and cheap. On the box every call is timed: a pack that takes more than 45 % of the block
period eight blocks in a row is bypassed (`tripped` in `resona-lab pack blocks`).

## Examples

In the SDK's `examples/` folder (`resona-lab templates --json` prints the SDK path):
`examples/tiny_synth` (an instrument: oscillator, filter, envelope), `examples/gain` (smallest), `examples/drive` (non-linear, no libm in the loop),
`examples/delay` (large host-owned state and a safe feedback path), `examples/widgets` (the UI
kit). All draw a screen. `tests/ui_stress` is not an example: it draws extreme coordinates, clamped
scales and more calls than the budget allows, and is the regression fixture for the
executor.

Explicit executable placement is described in [Flexible Code](../../examples/flexible_code/README.md). Code may use the dedicated fast pool or shared SRAM/PSRAM; state and buffers remain independently placed. Requires matching resource7 firmware.


### Preset-program development firmware

SDK dev.22 uses algorithm ABI2 and32-bit parameter counts in META. Rebuild user
algorithms for matching firmware. One preset contains one RPK (which may contain
many DSP blocks), independent float parameter values and optional shared home
artwork. Twelve is a delivery/page width, not the ABI2 metadata count. The visual
patch editor's12-control layout is separate. See
[compatibility](../../docs/compatibility.md#preset-program-backend) before device
installation; source updates alone do not change a connected Box.
