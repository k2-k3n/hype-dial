// The backing level must mute or boost the accompaniment without muting guitar.
#include <cassert>
#include <cmath>
#include "../dsp/src/dsp.cpp"

static double render(float backing, bool guitar) {
    Dsp d = {};
    dsp_init(d);
    dsp_param(d, P_ENERGY, 2);
    dsp_param(d, 12, backing); // BACKING is appended after existing parameters
    dsp_reset(d);
    double sum = 0;
    float in[64], out[64];
    for (int b = 0; b < 750; ++b) {
        for (int i = 0; i < 64; ++i) in[i] = guitar ? .18f * std::sin((b * 64 + i) * .057f) : 0;
        dsp_process(d, in, out, 64);
        for (float sample : out) { assert(std::isfinite(sample)); sum += sample * sample; }
    }
    return std::sqrt(sum / 48000);
}

int main() {
    assert(P_COUNT == 13);
    assert(kParams[12].def == 100.0f);
    assert(render(0, false) < 1e-5);
    assert(render(150, false) > .03);
    assert(render(0, true) > .02);
}
