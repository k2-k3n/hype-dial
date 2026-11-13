# Recipes

Small, complete changes. Each ends with the commands that prove it works.

## Packs

### Add a parameter
1. In `src/params.h` (effect and instrument templates), add a name to `enum Param` before
   `P_COUNT` and a line to `kParams` in the same place: name (at most 12 characters), min,
   max, default, unit. The screen grows a knob for it.
2. In `src/dsp.cpp`, handle it in `dsp_param()`: store what `dsp_process()` needs (a
   coefficient, a linear gain) in `struct Dsp`, computed there, not per sample.
3. Use it in `dsp_process()`. (Projects not made from a template: the `resona_param` table,
   `set_param()` and `process()` in the module.)
4. `resona-lab build . && resona-lab check . && resona-lab render . --param <index>=<value>`

### Add a filter, delay or LFO from the block library
`resona-lab block add filter --project .` copies the block into `blocks/`, declares it in
`src/dsp.h`, initialises it in `src/dsp.cpp`, and prints the line to put in `dsp_process()`.
Then `resona-lab flow .` shows the signal path, and `check` and `render` as above.

### Change the screen
Edit the drawing in `src/ui.cpp` (effect template; `ui_draw` in `src/main.cpp` calls it), then
`resona-lab ui-preview . --scale 2` and look at `build/ui.png`. Replay input with
`--event selector:click --event customize:turn:+3`. Keep within 128 draw calls per frame.

### Make it an instrument
Start from the `instrument` template or `examples/tiny_synth`: a NOTE and a GATE parameter
drive an oscillator, a filter and an envelope; the input is ignored or mixed in. A box app
(the `app-synth` template) sets NOTE and GATE from MIDI or a sequencer.

## Box apps

### Add a page
Keep a `view` variable in your state; give the new page an enum value, a draw function and
an input handler. In `event()`, route `RESONA_APP_EV_INPUT` by view; set a dirty flag and
redraw from `frame()`. In the factory app see `render_ui()` and `handle_input()` in
`src/factory.c`.

### Add a row to Settings (factory app)
Add an id to the `SETTING_*` enum and a name to `g_setting_names`, fill its value in
`format_setting_values()`, and change it in `edit_setting()` with `P->setting_set` (or your
own storage for an app-only setting). The BLOCK SIZE row is a short example.

### Map a MIDI CC
In `event()`, on `RESONA_APP_EV_MIDI` with `data[0] & 0xF0 == 0xB0`, read the controller
(`data[1]`) and value (`data[2]`) and call, for example,
`P->block_param_set(role, index, value * 100.0f / 127.0f, NULL)`.

### Remember something across reboots
Keep a small struct with a magic number; read it in `init()` with `app_store_read` (fall back
to defaults when the magic is wrong), write it with `app_store_write` when it changes.

Each: `resona-lab app build .`, then `resona-lab app install .` on a box (if the user wants it
installed), then `resona-lab app info`.
