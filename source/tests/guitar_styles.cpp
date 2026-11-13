// With the backing track stopped, each genre should shape the guitar differently.
#include <cassert>
#include <cmath>
#include <cstdio>
#include "../dsp/src/dsp.cpp"

int main() {
    constexpr int kFrames = 4096;
    float in[64], out[64], rendered[6][kFrames];
    for (int genre = 0; genre < 6; ++genre) {
        Dsp d = {};
        dsp_init(d);
        dsp_param(d, P_GENRE, static_cast<float>(genre));
        dsp_param(d, P_STOP, 1.0f);
        dsp_reset(d);
        for (int block = 0; block < kFrames / 64; ++block) {
            for (int i = 0; i < 64; ++i) {
                const int n = block * 64 + i;
                in[i] = .2f * std::sin(n * .073f) * (n < 2000 ? 1.0f : .5f);
            }
            dsp_process(d, in, out, 64);
            for (int i = 0; i < 64; ++i) {
                rendered[genre][block * 64 + i] = out[i];
                assert(std::isfinite(out[i]) && std::fabs(out[i]) < .95f);
            }
        }
    }
    for (int a = 0; a < 6; ++a) for (int b = a + 1; b < 6; ++b) {
        double sum = 0;
        for (int i = 0; i < kFrames; ++i) {
            const double v = rendered[a][i] - rendered[b][i];
            sum += v * v;
        }
        const double rms = std::sqrt(sum / kFrames);
        std::printf("guitar %d-%d diff %.5f\n", a, b, rms);
        assert(rms > .003);
    }
}
