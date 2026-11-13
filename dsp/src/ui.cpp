// ui.cpp: the screen. One knob per parameter, four to a page.
//
//   SELECTOR  turn: move the selection       click: next page
//   CUSTOMIZE turn: change the selected      click: press it     long press: reset it
//
// To draw something of your own, add to screen_draw(): the draw calls on `ui` are in
// <resona/module.h>, the ready-made widgets (Knob, Fader, Meter, Toggle, Steps, XYPad) in
// <resona/widgets.h>.
#include "ui.h"

void screen_init(Screen &s, const char *title)
{
    s.form.init(title);
    for (uint32_t i = 0; i < P_COUNT; ++i) {
        s.knob[i].init(46 + 76 * (int32_t)(i % 4u), 84, kParams[i].name, kParams[i].min,
                       kParams[i].max, kParams[i].def, kParams[i].unit);
        if (kParams[i].min > 0.0f && kParams[i].max >= 20.0f * kParams[i].min) {
            s.knob[i].log_scale();    // a wide positive range reads better on a log scale
        }
        s.form.add(&s.knob[i], (int)(i / 4u));
    }
    s.form.settle();
}

void screen_set(Screen &s, uint32_t index, float value)
{
    if (index < P_COUNT) {
        s.knob[index].set(value);
    }
}

void screen_tick(Screen &s)
{
    s.form.tick();
}

void screen_draw(const Screen &s, const resona_ui *ui)
{
    s.form.draw(ui);
}

int screen_event(Screen &s, const resona_event &e)
{
    return s.form.handle(e);
}

float screen_value(const Screen &s, uint32_t index)
{
    return s.knob[index].value();
}
