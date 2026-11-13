// At the startup ENERGY setting, each genre needs its own audible groove.
#include <cassert>
#include <cstdio>
#include "../dsp/src/dsp.cpp"

struct Groove { unsigned kick, snare, hat, bass, arp; };

static Groove groove(int genre) {
    Dsp d = {};
    dsp_init(d);
    dsp_param(d, P_GENRE, static_cast<float>(genre));
    d.energy = 2.0f;
    Groove g = {};
    for (int i = 0; i < 16; ++i) {
        d.kick = d.snare = d.hat = d.clap = d.bass = d.arp = 0;
        beat(d);
        if (d.kick > 0.1f) g.kick |= 1u << i;
        if (d.snare > 0.1f || d.clap > 0.1f) g.snare |= 1u << i;
        if (d.hat > 0.1f) g.hat |= 1u << i;
        if (d.bass > 0.1f) g.bass |= 1u << i;
        if (d.arp > 0.1f) g.arp |= 1u << i;
    }
    return g;
}

int main() {
    assert(kParams[P_GENRE].max == 5);
    Groove g[6];
    for (int i = 0; i < 6; ++i) g[i] = groove(i);
    for (int i = 0; i < 6; ++i) {
        assert(g[i].kick != 0);
        for (int j = i + 1; j < 6; ++j)
            assert(g[i].kick != g[j].kick || g[i].snare != g[j].snare || g[i].hat != g[j].hat);
    }
    assert(g[2].snare != 0); // Hip-hop backbeat
    assert(g[3].snare != 0); // DnB backbeat
    assert(g[4].snare != 0); // Funk backbeat and ghost notes
    assert(g[0].bass == 0 && g[0].arp != 0); // House stays airy but has a soft melody at ENERGY 2
    assert(g[1].bass != 0); // Techno gains a low pulse
    assert(g[2].bass != 0 && g[2].bass != g[1].bass); // Hip-hop sub follows its kick
    assert(g[3].arp != 0); // DnB gains fast bright notes
    assert(g[4].bass != 0 && g[4].bass != g[2].bass); // Funk bass is syncopated differently
    assert(g[5].snare != 0 && g[5].bass != 0); // Latin conga and tumbao
    for (int i = 0; i < 6; ++i) assert(g[i].arp != 0); // every style has a motif at the default level
    for (int i = 0; i < 6; ++i)
        std::printf("genre %d: kick=%04x snare=%04x hat=%04x bass=%04x arp=%04x\n",
                    i, g[i].kick, g[i].snare, g[i].hat, g[i].bass, g[i].arp);
}
