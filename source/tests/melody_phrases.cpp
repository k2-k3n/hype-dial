// At the default energy, all six genres must have their own changing four-bar phrase.
#include <cassert>
#include <cstdint>
#include <cstdio>
#include "../dsp/src/dsp.cpp"

static uint32_t phrase(int genre, int bar) {
    Dsp d = {};
    dsp_init(d);
    dsp_param(d, P_GENRE, static_cast<float>(genre));
    d.energy = 2.0f;
    d.bar = static_cast<uint32_t>(bar);
    d.step = 0;
    d.arp = 0;
    beat(d);
    uint32_t sig = 2166136261u;
    auto hash = [&](int n) { sig = (sig ^ static_cast<uint32_t>(n)) * 16777619u; };
    for (int i = 0; i < 8; ++i) {
        hash(static_cast<int>(d.bassNotes[i] * 1000000));
        hash(static_cast<int>(d.arpNotes[i] * 1000000));
    }
    hash(d.arp > 0.1f ? 1 : 0);
    return sig;
}

int main() {
    uint32_t p[6][4] = {};
    for (int g = 0; g < 6; ++g) {
        for (int b = 0; b < 4; ++b) p[g][b] = phrase(g, b);
        int unique = 0;
        for (int b = 0; b < 4; ++b) {
            bool seen = false;
            for (int i = 0; i < b; ++i) seen |= p[g][i] == p[g][b];
            unique += !seen;
        }
        std::printf("genre %d unique phrases %d\n", g, unique);
        assert(unique >= 3);
    }
    for (int a = 0; a < 6; ++a) for (int b = a + 1; b < 6; ++b)
        assert(p[a][0] != p[b][0]);
}
