# Writing a box app

A box app is the Resona Box's application: what the screen shows, what the encoders do, how
presets are browsed, which MIDI messages mean what, the settings pages. The platform (the
closed firmware under it) keeps boot and recovery, the audio thread and the factory engines,
algorithm packs, storage and its keys, and the install channel. The app talks to the platform
only through `resona_platform` in `sdk/include/resona/platform.h`; read that header for the
exact signatures. This guide says how the parts fit together.

Start from a template: `app-factory` is the box's real UI (pages, presets, settings, Global
EQ, Encoder 2 mapping) and the best reference for how the API is used; `app-blank` is the
smallest working app; `app-synth` drives an algorithm pack as an instrument.

## Creator Lab workflow

Use New Project → Box App, choose a template, and edit its C/C++ files in the normal
explorer. Build checks the RV32 package and starts the full interactive device UI preview.
Both encoders, timers, settings and temporary local presets are available. Install builds
and reviews a package, then writes the app partition of exactly Resona Box `33C3:1307` and
restarts it. No CLI or OS/bootloader flashing is required; existing presets/models survive.

Preview is a separate host process: crashes/timeouts preserve the editor. Its storage is
local and temporary, its asset library starts empty, and it does not simulate DSP,
model/pack loading, pack screens, transport or threads. Synth Box therefore shows its
missing-pack state locally; test instrument playback on a box with Tiny Synth installed.
The worker has normal host permissions; it is not a security sandbox. Restart/Build resets
local simulation state. Use the real box for hardware/CPU/audio checks.

## How an app runs

```c
static const resona_app kApp = {
    RESONA_PLATFORM_VERSION, sizeof(resona_app),
    "org.example.my-app", "My App", "0.1.0",
    sizeof(State),   /* the platform allocates this, zeroed, and passes it back */
    64 * 1024,       /* the most you will ask from P->alloc (at most 512 KiB) */
    init, event, frame,
};
const resona_app *resona_app_get(void) { return &kApp; }   /* extern "C" in C++ */
```

- At boot the platform loads the app into memory, runs C++ static constructors, then
  `init(state, platform)`. Keep the `platform` pointer (`P` below). Return 0, or the box starts
  in safe mode.
- `event(state, ev)` gets every event in order: input gestures, MIDI, changes made by
  someone else, timers, audio restarts.
- `frame(state, ms)` runs about 30 times a second: animate and redraw what changed.
- All three run on the app's **main thread**. They must return quickly: no waiting, no long
  loops. Use `timer_start` for bounded periodic work (at most8 timers; all32-bit IDs
  are valid). Periods1..2147483647ms; invalid periods return ERR_ARG without
  replacing an existing timer. Exhaustion returns ERR_NO_MEMORY; repeat0 means forever.
  Background threads and RTOS mutexes are unsupported in the protected runtime.
- Apps may use mutable globals and C++ constructors (unlike packs). No exceptions or RTTI.
- No C library beyond `resona/libc.h` (memcpy … snprintf). Memory comes from `P->alloc`: power-of-two alignment1..4096, at most64 live allocations on the device; NULL on invalid requests or exhaustion. The byte budget and alignment padding also constrain fit.
- Limits on the Resona Box: image (code + data) 192 KiB, heap 512 KiB, event queue 32
  events (more are dropped), MIDI/SysEx messages up to 256 bytes per event.
  `post()` copies the event and valid MIDI/CTRL payload before returning. Payload
  pointers received by `event()` are valid only during that callback. The queue
  includes an event being dispatched and is shared with platform notifications;
  use an App acknowledgement protocol when reliable delivery matters.

## Safety

- An app that crashes or fails `init` while starting is marked failed; the next boot shows the
  platform's **safe mode** (it says why). Holding **Encoder 1 at power-on** also gives safe
  mode for that boot. Installing a new app always works from safe mode.
- `resona-lab app info` shows what runs and why; `resona-lab app reboot --safe` gives one
  safe-mode boot.

## Screen

320 × 172 pixels, RGB565 (`RESONA_RGB(r, g, b)`).

```c
uint16_t *px; uint32_t stride;
resona_ui *ui = P->screen_begin(&px, &stride);     /* the back buffer */
ui->clear(ui->ctx, RESONA_RGB(10, 12, 18));
ui->text(ui->ctx, 12, 10, 3, RESONA_RGB(255, 160, 0), "HELLO");
px[y * stride + x] = colour;                      /* direct pixels are fine too */
P->screen_present(0, 0, 0, 0);                    /* 0,0,0,0 = the whole screen */
```

`resona_ui` has clear, rect, text (5×7 font, scale 1–8), line, dotline, ellipse,
ellipse_fill, dots, poly, poly_fill (`resona/ui.h` wraps them). Present only the rectangle
you changed when you redraw often (meters): it is cheaper. A block holding a pack with its
own screen can draw it with `block_screen_draw(role, force)` and pass encoder events to it with
`block_screen_event`.

### App-owned fonts and UTF-8

Built-in text is a small5×7 face, with one fallback per unsupported Unicode
scalar. Put your own font assets directly in the App project and draw them
with `<resona/font.h>` / `resona_ui_text_font`. `tools/font.py` converts selected
glyphs from a licensed TTF/OTF into a C header, keeping its notice beside it.
No source TTF, Python or font engine is needed on the Box. Both Apps and block
screens can use the helper; block draw budgets still apply. See the standalone
App-owned Unicode font example. LVGL objects inside the OS are private; Apps
may supply their own rendering/layout integration using the framebuffer.

## Input

Two push-encoders: `RESONA_INPUT_SELECTOR` (Encoder 1, top) and `RESONA_INPUT_CUSTOMIZE`
(Encoder 2, bottom). `RESONA_APP_EV_INPUT` carries a gesture in `ev->u.input`:

| `kind` | meaning |
| --- | --- |
| `RESONA_EVENT_TURN` | `value` = signed detents (+ clockwise) |
| `RESONA_EVENT_CLICK` | a short press (on release, unless press-on-down) |
| `RESONA_EVENT_DOUBLE` | a double press, only if enabled for that encoder |
| `RESONA_EVENT_LONG` | held about 0.6 s |

`P->input_config(input, flags)`: `RESONA_INPUT_DOUBLE` reports double presses (single clicks
then wait about 250 ms); `RESONA_INPUT_PRESS_ON_DOWN` reports a click on press. Raw
press/release arrives as `RESONA_APP_EV_KEY`. The status light: `P->led_set(RESONA_LED_*)` for legacy status modes.

API5 adds full RGB through `P->rgb_set(&config)` and `P->rgb_get(&state)`:

```cpp
resona_rgb_config c = resona_rgb_solid(255, 30, 80, 128);
c.animation = RESONA_RGB_BREATHE;
c.period_ms = 1800;
P->rgb_set(&c);
```

Check version>=5 and size through rgb_get before reading the tail. Brightness
and colors use linear0..255 intensity. Solid uses period0; other animations
use20..60000ms. Blink/fade use secondary RGB; rainbow ignores base colors; fade
holds the secondary color after one period. NULL releases custom control.
Animation scheduling and PWM are trusted platform services, outside audio.
Do not call these APIs in algorithm process(). A Lab override returns ERR_STATE
to app writes until released; fault recovery restores safe legacy control.
See the RGB Studio example for encoder/MIDI control and custom frame sequences.

## The sound: blocks

Five block positions (roles) in a chain; roles keep their identity when reordered:

| role | factory engine | parameters (own units) |
| --- | --- | --- |
| 0 GATE | gate (variant 0) / auto gate (1) | threshold, release (0–100) |
| 1 COMP | compressor | threshold, ratio, knee, attack, release, makeup (0–100) |
| 2 RESONA | amp profile (EAD model) | gain, bass, mid, treble, level (0–100) |
| 3 NAM | NAM A2 Lite (NAM model) | level, bass, mid, treble (0–100) |
| 4 CAB | impulse response (IR) | low cut, high cut (0–100), level (−12…+12 dB) |

- `block_info(role, &info)`: engine running (`RESONA_ENGINE_*`, `RESONA_ENGINE_PACK` for a pack),
  on/off, variant, selected model, parameter count, name, `tripped` (a pack bypassed for
  taking too long).
- `block_enable`, `block_variant`, `block_param_get/set` (in the units above; for a pack, its
  own units — read them with `block_param_info`), `block_order_get/set`.
- Models: `entry_count` / `entry_info(position)` list stored EAD, NAM, IR models and packs;
  `block_model_select(role, entry_id)` loads one (asynchronous: wait for
  `RESONA_CHANGED_BLOCK`).
- Packs: `block_pack_load(role, entry_id)` runs an installed pack in a position,
  `block_pack_clear(role)` brings the factory engine back. Installing packs is Lab's job.

## Presets

50 slots (`preset_count`). The platform captures and applies the whole sound; the app decides
when. `preset_preview(slot)` plays a slot without making it active (browsing),
`preset_load(slot)` makes it active, `preset_save(slot, name)` (NULL keeps the name),
`preset_active()` (−1: the init sound), `preset_default()`, `preset_info(slot)`,
`preset_live_name`.

## Settings and global audio

API4 adds validated [audio routing and mixers](audio-routing.md), including guitar,
USB and Bluetooth sources, explicit L/R mapping, slot connections and independent
USB recording. Older table entries keep their offsets; check version/size before
using `audio_routing_set/get`.

`setting_get/set(RESONA_SETTING_*, value)`: VOLUME 0–100, BYPASS, INPUT_GAIN −20…20 dB,
LOOPBACK, USB_VOLUME, BT_ENABLE, BT_VOLUME, MIDI_CHANNEL (0 = omni), BLOCK_FRAMES 64/128/256
(restarts audio; you get `RESONA_APP_EV_AUDIO` when it is back). Global EQ:
`global_eq_get/set` (five bands 100 Hz–4 kHz and a level, in half dB; `persist` 0 while a
control turns, 1 when it is let go). Both enabled and persist must be0or1;
invalid complete requests return ERR_ARG before changing any EQ field. With
persist0, enable changes as well as bands/level are temporary. `meters(&in, &out, &cpu)`: levels 0–9.

## MIDI and SysEx

MIDI reaches the app as `RESONA_APP_EV_MIDI` (`port` USB, BLE or UART; complete
SysEx frames). Messages over256 bytes are dropped whole. Send1..512 bytes with
`P->midi_send(port, data, len)`; invalid scalar length/port returns ERR_ARG without
faulting the App. Inaccessible user buffers remain protected memory faults. SysEx frames `F0 7D 4E 43 …` carry
the box protocol; the platform's own ops (0x01, 0x02, 0x80–0x95: identify, reboot, packs,
app install) are reserved and never reach the app. Anything else is yours to define.

## Changes made by someone else

`RESONA_APP_EV_CHANGED` with `what` = `RESONA_CHANGED_PRESET` (a host recalled a preset),
`BLOCK`, `ORDER`, `MODELS` (models or packs installed/removed), `GLOBAL`, `WIRELESS`, `USB`.
Re-read what you show and redraw. `RESONA_APP_EV_AUDIO`: audio restarted (`block_frames`,
`sample_rate`) or a pack was bypassed by its time guard (`tripped_role` ≥ 0).

## Storage

`app_store_read/write(offset, buf, len)`: the app's own bytes (`app_store_size()`, 4 KiB on
the Resona Box), power-safe, kept across reboots and updates of the same app id.
An invalid offset/length, including unsigned overflow, returns `RESONA_ERR_ARG`
and leaves destination/storage unchanged. Empty spans at offset==size are valid
no-ops with a valid non-NULL buffer. The protected device still faults an App
that supplies inaccessible memory for a valid nonempty span. Use
`resona_app_store_range_valid(capacity,offset,length)` for optional caller checks.
This error-return behavior requires the matching storage-range firmware fix;
SDK headers alone cannot update an older device. Put a magic
number and version at the start and validate on read. Presets, models and settings are
stored by the platform; do not copy them here.

## Not available yet

`ctrl_send` (custom wireless controller packets) returns `RESONA_ERR_UNSUPPORTED`; renaming, recolouring and
deleting models works for some entry kinds only (check the return value). Signing is not
enforced yet: every box accepts unsigned apps.

## Custom chains (platform API 2)

`P->chain_config(1, 1)` enters custom mode with one empty/dry slot. Add slots explicitly with
`P->chain_config(1, n)` (1..5), or return to factory with `(0, 1)`. Mode changes clear running
RAM packs, not installed files. `P->chain_info(&info)` gives live audio ticks (4 MHz), fast and
PSRAM heap capacity/free bytes and instruction-memory use. The platform measures and admits
packs before binding them; treat a refusal as a real resource limit. Factory memory is reserved by the platform; pack state never silently falls back to normal RAM. CPU trial results are measurements,
not a hard execution-time guarantee; the protected runtime separately contains access faults and stuck callbacks. CPU trials cover all 64/128/256-frame sizes so a block-size change keeps the reservation valid. Refresh an older project's pinned SDK with `resona-lab sdk sync`.

## Code-first startup customization (API6)

Declare USB product/audio/MIDI names in your Box App code using
`resona/startup_helpers.h` and `resona_startup_usb_apply(platform,&names)`.
Negative returns refuse the change;1 means saved/reboot required;0 means active.
Only call reboot when required, so initialization cannot loop resets. There is
no USB naming window or user-supplied VID/PID. The trusted platform manages
profiles; Creator Lab reconnects through reserved CDC, independent of MIDI names.
Names are1..47 UTF-8 bytes without control characters. Runtime support is needed.

`startup_splash_set(data,bytes)` installs a validated independent RSP1 animation;
NULL/0 restores the factory splash. Duration0 skips; other durations are chosen
by the author. Use the SDK tools/splash.py tool and docs/startup.md, or generate
the public format from code. Long splashes delay Box App initialization and preset
recall; USB management remains available. Use resona/splash.h to build frames and resona_startup_splash_apply to stage only changed assets; there is no startup setup window.
Do not put any startup operation in an audio callback. Do not repeatedly write
settings every frame. All startup setters are bounded protected control calls.

API7 adds `bluetooth_names_get/set` and `<resona/bluetooth_helpers.h>`. Name changes persist in the wireless controller and require disconnecting all power. No radio address, bond or sample-rate control is exposed.


### Resource-admitted effect graphs (API8)

Use `<resona/effects.hpp>` and one `resona::Effects` controller for an App-owned
graph. `begin(platform)` transfers audio ownership; `factory(engine)` and
`pack(id,version,crc32)` return independent handles. `connect(source,destination,gain,
source_channel,destination_channel)`, `output(handle)` and `capture(handle)` build
routes; `commit()` validates, admits and copies them. `param`, `enable`, `info`,
`param_info` and `status` operate on handles. Check every returned error/zero handle.
Use `clear_routes()` to build a replacement graph. CPU/RAM/code determine the
instance count; ten heavy effects may not fit. USB/Bluetooth sources are stereo,
effects are mono, and summing is explicit. A failed commit retains the old graph.
See `docs/compatibility.md` for lifetime, preview and firmware requirements.

SDK dev.16 publishes these scalar limits and requires matching platform firmware.
An active effect graph may commit a lower/equal mixer reservation even when the
latest platform peak has risen, so routes can be removed before destroying an
instance. First activation and reservation increases still require CPU headroom.

The non-increasing graph admission exception also requires that the new plan
uses only DSP nodes already present in the active plan. Newly activated nodes
retain normal headroom checks, even if their instances are already resident.

SDK dev.20 preview/native factory metadata uses the same public descriptor
table, including Auto Gate SENSITIVITY. `effect_param_info.def` stays constant
after control changes. Failed pack creation distinguishes missing ID, version
mismatch, CRC mismatch, ambiguity, malformed stored data and I/O; see the
ERR_PACK_* constants in platform.h. `resona_error_string(rc)` gives a display
label. New device codes need matching firmware; the API8 table is unchanged.
