// With no guitar input, each genre must produce backing across the full ENERGY range.
#include <cassert>
#include <cmath>
#include <cstdio>
#include <initializer_list>
#include "../dsp/src/dsp.h"

int main() {
    for (int genre = 0; genre < 6; ++genre) {
        for (int energy : {0, 1, 2, 5, 10}) {
            Dsp d = {};
            dsp_init(d);
            dsp_param(d, P_GENRE, static_cast<float>(genre));
            dsp_param(d, P_ENERGY, static_cast<float>(energy));
            dsp_param(d, P_BACKING, 100);
            dsp_reset(d);
            float input[64] = {}, output[64];
            double sum = 0;
            for (int block = 0; block < 750; ++block) {
                dsp_process(d, input, output, 64);
                for (float sample : output) {
                    assert(std::isfinite(sample));
                    sum += sample * sample;
                }
            }
            const double rms = std::sqrt(sum / 48000);
            std::printf("genre %d ENERGY %d backing RMS %.5f\n", genre, energy, rms);
            assert(rms > .002);
        }
    }
}
