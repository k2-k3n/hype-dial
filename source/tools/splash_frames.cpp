// MIT. Renders HYPE DIAL's boot-splash frames with the app's own block grotesque (src/type.h) and colours (host
// build, see make_splash.py). Raw RGB565 320x172, in order: black; the four genre accents as departure-board bands,
// three rotations; a HYPE slam on acid yellow; a DIAL slam on safety orange; the attract screen (HOUSE 124, 02,
// the layer roster) on the plain ground (the 8 px grid would cost ~13,000 runs, far over the 24 KiB splash budget).
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <initializer_list>
#include <resona/ui.h>
#include "type.h"
#define C565(hex) RESONA_RGB(((hex) >> 16) & 255, ((hex) >> 8) & 255, (hex) & 255)
static uint16_t fb[320 * 172];
static const uint32_t kAcc[4] = {0xE4FF1A, 0xFF5A1F, 0xFF2A3D, 0x19E3FF};
int main(int argc, char **argv) {
 const uint16_t BG = C565(0x0A0A0A), GRID = C565(0x1A1A19), WHITE = C565(0xF2F2EE), CONC = C565(0x8A8A86), DARK = C565(0x3A3A38), INK = C565(0x0A0A0A);
 static uint8_t pool[200 * 1024]; type::Pool P{pool, 0, sizeof pool};
 type::Face num, w16, w12, m10, m8; type::face(num, 150, 72, 0); type::face(w16, 16, 9, 3); type::face(w12, 12, 7, 2); type::face(m10, 10, 6, 2); type::face(m8, 8, 5, 2);
 type::Target T{fb, 320, 0, 0, 320, 172};
 FILE *f = fopen(argc > 1 ? argv[1] : "frames.raw", "wb");
 auto out = [&] { fwrite(fb, 2, 320 * 172, f); };
 type::rect(T, 0, 0, 320, 172, BG); out();                                             // 0 black
 for (int rot = 0; rot < 3; ++rot) {                                                     // 1-3 the bands
  for (int i = 0; i < 4; ++i) type::rect(T, 0, i * 43, 320, 43, C565(kAcc[(i + rot) % 4]));
  out();
 }
 auto slam = [&](const char *word, uint32_t bg) {                                        // 4-5 the slams
  type::rect(T, 0, 0, 320, 172, C565(bg));
  int w = 0; for (const char *c = word; *c; ++c) w += type::get(num, P, *c).w + 4; w -= 4;
  int x = (320 - w) / 2; for (const char *c = word; *c; ++c) { type::Mask m = type::get(num, P, *c); type::stamp(T, m, x, 11, INK); x += m.w + 4; }
  out();
 };
 slam("HYPE", kAcc[0]); slam("DIAL", kAcc[1]);
 // 6 the attract screen: accent column, 02, the roster
 type::rect(T, 0, 0, 320, 172, BG); (void)GRID;
 type::rect(T, 0, 0, 24, 172, C565(kAcc[0])); type::rect(T, 24, 0, 2, 172, CONC);
 type::textUp(T, w16, P, "HOUSE", 4, 160, INK); type::textUp(T, w12, P, "124", 6, 40, INK);
 type::stamp(T, type::get(num, P, '0'), 34, 6, CONC); type::stamp(T, type::get(num, P, '2'), 114, 6, WHITE);
 const char *layers[7] = {"PAD", "HATS", "KICK", "BASS", "CLAP", "ARP", "STAB"};
 type::tiny(T, "LAYERS", 196, 4, CONC);
 for (int i = 0; i < 7; ++i) {
  const int y = 16 + 18 * i; type::text(T, m10, P, layers[i], 196, y + 1, WHITE);
  if (i < 2) type::rect(T, 240, y, 76, 12, C565(kAcc[0])); else type::frame(T, 240, y, 76, 12, DARK);
 }
 // the energy ticks: only the three lit ones (the eight dark outlines would cost the last ~800 bytes)
 for (int i = 0; i <= 2; ++i) type::rect(T, 34 + 14 * i, 160, 12, 10, i < 2 ? WHITE : C565(kAcc[0]));
 out();
 fclose(f);
}
