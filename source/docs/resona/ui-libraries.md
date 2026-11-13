# UI libraries for Box Apps

The Resona OS has no UI toolkit: an App draws its 320 x 172 RGB565 frame into the back buffer
from `P->screen_begin` and shows it with `P->screen_present`. A toolkit is a library compiled
into the App, so the App pays for it in size. The SDK offers two, both optional, in `lib/`:

| | `lib/widgets` (Box UI) | `lib/lvgl_port` (LVGL 9) |
| --- | --- | --- |
| Look | the Box's own screens: pixel font, factory palette | LVGL's default theme (dark), Montserrat 14 |
| Widgets | text (scale 1-3), header, value tile, bar, meter, page dots, list, toast, encoder focus | label, button, bar, slider, list, image; more by enabling them |
| App code | about 4 KB (`widgets_demo`: 4.9 KB .rpk) | about 150 KB (`lvgl_demo`: 154 KB .rpk) |
| RAM | none of its own; widget state lives in your State | 40 KB LVGL heap + 15 KB render band from `P->alloc` |
| Language | C (usable from C++) | C (usable from C++) |

Apps may be up to 512 KB; a Box keeps about 320 KB of user area free, so an LVGL App leaves
less room for other Apps and assets. Prefer Box UI unless you need LVGL's layouts, styles or
animations.

## Using a library

The App build compiles exactly the files listed in `sources` of `resona.project.json`. Library
sources are not part of the pinned SDK (`include/`, `device/`), so list them by path. The
examples in `apps/` reach them inside the SDK folder:

```json
"sources": ["src/app.c", "../../lib/widgets/box_ui.c"]
```

For a project outside the SDK folder, copy `lib/widgets` (or `lib/lvgl_port`, keeping
`lvgl/LICENCE.txt`) into the project, e.g. as `lib/widgets`, and list `lib/widgets/box_ui.c`.

## Box UI (`lib/widgets/box_ui.h`)

```c
boxui_canvas c;
boxui_begin(&c, P);                                   /* back buffer from screen_begin */
boxui_clear(&c, BOXUI_BG);
boxui_header(&c, "AMP", "1/2");
boxui_tile(&c, 8, 40, 98, 104, "GAIN", "+6", "DB", 750, boxui_focus_mode(&s->focus, 0));
boxui_page_dots(&c, 160, 160, 0, 2);
boxui_toast_draw(&c, &s->toast);
boxui_present(&c, P);
```

`boxui_focus_handle(&focus, ev, &delta)` turns encoder events into `BOXUI_MOVED` (E1 turn),
`BOXUI_ADJUST` (E2 turn, or E1 while `focus.editing`), `BOXUI_ACTIVATE` (click) and
`BOXUI_BACK` (E1 long). `boxui_list` scrolls a string list; `boxui_toast` times itself out
with `boxui_toast_tick(&t, ms)` from `frame`. Nothing allocates. Text is the 5x7 ASCII face;
for other scripts use an App-owned font (`resona/font.h`). Example: `apps/widgets_demo`.

## LVGL port (`lib/lvgl_port/lvbox.h`)

```c
static int init(void *s, const resona_platform *P) {
    if (lvbox_init(P, 0) != RESONA_OK) return -1;          /* lv_init, display, encoders */
    lv_obj_t *slider = lv_slider_create(lv_screen_active());
    lv_group_add_obj(lvbox_group(RESONA_INPUT_CUSTOMIZE), slider);   /* E2 turns it */
    lv_list_add_button(lv_list_create(lv_screen_active()), NULL, "Clean"); /* E1 group */
    lvbox_refresh();
    return 0;
}
static void event(void *s, const resona_app_event *ev) { lvbox_event(ev); }
static void frame(void *s, uint32_t ms) { lvbox_frame(); }      /* lv_timer_handler */
```

- Display: partial rendering in 24-row bands; the flush copies each band into the back
  buffer and presents the changed rectangle once per refresh.
- Tick: `P->millis`. Heap: LVGL's builtin TLSF allocator on a pool from `P->alloc`
  (`LV_MEM_SIZE`); set the App's `heap_bytes` to at least `lvbox_heap_bytes(rows)` (64 KB in
  the demo). No C library is needed: LVGL's builtin string and sprintf are used.
- Input: E1 drives the default group (turn = focus, click = press / edit toggle,
  long = `LV_KEY_ESC`); E2 drives a second group kept in edit mode, so its turns change the
  object directly.
- `lv_conf.h` enables only what the demo uses. To add a widget, set its `LV_USE_*` to 1 and
  add its `lvgl/src/widgets/<name>/*.c` (from LVGL 9.2.2) to `sources`; check the .rpk size.
- LVGL is MIT; its Montserrat/Font Awesome bitmap font is SIL OFL 1.1. See
  `THIRD_PARTY_NOTICES.md`.

Example: `apps/lvgl_demo` (its `sources` list is the full LVGL file set the port needs).
Preview either with `resona-lab app preview <project> --out frame.png --event selector:turn:1`.
