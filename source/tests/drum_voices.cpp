// Each genre needs a distinct kick, snare, and hat sound, beyond BPM and event placement.
#include <cassert>
#include <cmath>
#include <cstdio>
#include "../dsp/src/dsp.cpp"

static void voice(int genre, int kind, float *out) {
    Dsp d = {};
    dsp_init(d);
    dsp_param(d, P_GENRE, static_cast<float>(genre));
    dsp_param(d, P_ENERGY, 0.0f);
    dsp_param(d, P_LEVEL, 100.0f);
    dsp_reset(d);
    d.clock = 1;
    d.stepLength = 8000; // no sequencer hit during the isolated drum render
    if (kind == 0) d.kick = 1.0f;
    if (kind == 1) d.snare = .7f;
    if (kind == 2) d.hat = .6f;
    float in[64] = {}, block[64];
    for (int b = 0; b < 16; ++b) {
        dsp_process(d, in, block, 64);
        for (int i = 0; i < 64; ++i) {
            out[b * 64 + i] = block[i];
            assert(std::isfinite(block[i]) && std::fabs(block[i]) < .95f);
        }
    }
}

int main() {
    float rendered[6][1024];
    for (int kind = 0; kind < 3; ++kind) {
        for (int genre = 0; genre < 6; ++genre) voice(genre, kind, rendered[genre]);
        for (int a = 0; a < 6; ++a) for (int b = a + 1; b < 6; ++b) {
            double sum = 0;
            for (int i = 0; i < 1024; ++i) {
                const double diff = rendered[a][i] - rendered[b][i];
                sum += diff * diff;
            }
            const double rms = std::sqrt(sum / 1024);
            std::printf("drum %d genres %d-%d diff %.5f\n", kind, a, b, rms);
            assert(rms > .003);
        }
    }
}
