// Hype Dial type: an original ultra-condensed block grotesque on a 3 x 5 cell grid (stem | counter | stem by
// bar | counter | bar | counter | bar), stems 31 % of the width, bars 12 % of the height, one outer radius per
// bowl, square counters, a few diagonal strokes. 1-bit masks only, no anti-aliasing. Glyphs are generated once
// into the app heap at the sizes the screen uses. MIT.
#pragma once
#include <stdint.h>

namespace type {
// rows of the 3 x 5 cell grid: '#' filled, '.' empty, '1'..'4' filled with a rounded outer corner
// (1 top-left, 2 top-right, 3 bottom-right, 4 bottom-left)
struct Recipe { char ch; const char *rows[5]; };
static const Recipe kRecipes[] = {
 {'0', {"1#2", "#.#", "#.#", "#.#", "4#3"}}, {'1', {"1#.", ".#.", ".#.", ".#.", "###"}}, {'2', {"1#2", "..#", "1#3", "#..", "###"}},
 {'3', {"1#2", "..#", ".##", "..#", "4#3"}}, {'4', {"#.#", "#.#", "4##", "..#", "..#"}}, {'5', {"###", "#..", "##2", "..#", "4#3"}},
 {'6', {"1#2", "#..", "##2", "#.#", "4#3"}}, {'7', {"###", "...", "...", "...", "..."}}, {'8', {"1#2", "#.#", "###", "#.#", "4#3"}},
 {'9', {"1#2", "#.#", "4##", "..#", "4#3"}},
 {'A', {"1#2", "#.#", "###", "#.#", "#.#"}}, {'B', {"##2", "#.#", "###", "#.#", "##3"}}, {'C', {"1#2", "#..", "#..", "#..", "4#3"}},
 {'D', {"##2", "#.#", "#.#", "#.#", "##3"}}, {'E', {"###", "#..", "##.", "#..", "###"}}, {'F', {"###", "#..", "##.", "#..", "#.."}},
 {'H', {"#.#", "#.#", "###", "#.#", "#.#"}}, {'I', {"#", "#", "#", "#", "#"}}, {'K', {"#..", "#..", "#..", "#..", "#.."}},
 {'L', {"#..", "#..", "#..", "#..", "###"}}, {'N', {"#.#", "#.#", "#.#", "#.#", "#.#"}}, {'O', {"1#2", "#.#", "#.#", "#.#", "4#3"}},
 {'P', {"##2", "#.#", "##3", "#..", "#.."}}, {'R', {"##2", "#.#", "##3", "#.#", "#.#"}}, {'S', {"1##", "#..", "4#2", "..#", "##3"}},
 {'T', {"###", ".#.", ".#.", ".#.", ".#."}}, {'U', {"#.#", "#.#", "#.#", "#.#", "4#3"}}, {'G', {"1#2", "#..", "#.#", "#.#", "4##"}},
 {'M', {"#.#", "#.#", "#.#", "#.#", "#.#"}}, {'Y', {"#.#", "#.#", "4##", "..#", "##3"}}, {'V', {"#.#", "#.#", "#.#", "#.#", "4#3"}},
 {'-', {"...", "...", "###", "...", "..."}}, {'%', {"#.#", "..#", ".#.", "#..", "#.#"}}, {'/', {"..#", "..#", ".#.", "#..", "#.."}},
 {'=', {"...", "###", "...", "###", "..."}}, {' ', {"...", "...", "...", "...", "..."}},
};

inline int rnd(float x) { return static_cast<int>(x + (x >= 0 ? .5f : -.5f)); }   // JS Math.round for our ranges
inline const Recipe &recipe(char c) {
 for (const Recipe &r : kRecipes) if (r.ch == c) return r;
 return kRecipes[sizeof(kRecipes) / sizeof(kRecipes[0]) - 1];
}
inline bool inPoly(float px, float py, const float *q, int n) {
 bool c = false;
 for (int i = 0, j = n - 1; i < n; j = i++) {
  const float xi = q[i * 2], yi = q[i * 2 + 1], xj = q[j * 2], yj = q[j * 2 + 1];
  if ((yi > py) != (yj > py) && px < (xj - xi) * (py - yi) / (yj - yi) + xi) c = !c;
 }
 return c;
}

struct Mask { uint16_t w, h; const uint8_t *m; };

// The glyph `c` at w x h into `out` (w x h bytes, or v x h for one-column glyphs); returns its width.
inline int build(char c, int w, int h, uint8_t *out) {
 const Recipe &rc = recipe(c);
 int nc = 0; while (rc.rows[0][nc]) ++nc;
 const int hz = rnd(h * .12f) > 2 ? rnd(h * .12f) : 2, v = rnd(w * .31f) > 2 ? rnd(w * .31f) : 2;
 if (nc == 1) w = v;
 const int cw[3] = {v, w - 2 * v, v};
 const int rb[6] = {0, hz, rnd((h - hz) / 2.f), rnd((h + hz) / 2.f), h - hz, h};
 int cb[4] = {0, 0, 0, 0}; for (int i = 0; i < nc; ++i) cb[i + 1] = cb[i] + (nc == 1 ? v : cw[i]);
 for (int i = 0; i < w * h; ++i) out[i] = 0;
 const int R = rnd(v * .9f) > 2 ? rnd(v * .9f) : 2;
 for (int r = 0; r < 5; ++r) for (int k = 0; k < nc; ++k) {
  const char t = rc.rows[r][k]; if (t == '.' || t == ' ') continue;
  for (int y = rb[r]; y < rb[r + 1]; ++y) for (int x = cb[k]; x < cb[k + 1]; ++x) out[y * w + x] = 1;
 }
 for (int r = 0; r < 5; ++r) for (int k = 0; k < nc; ++k) {
  const char t = rc.rows[r][k]; if (t < '1' || t > '4') continue;
  const bool left = t == '1' || t == '4', top = t == '1' || t == '2';
  const int ox = left ? cb[k] : cb[k + 1], oy = top ? rb[r] : rb[r + 1];
  const float cx = left ? ox + R : ox - R, cy = top ? oy + R : oy - R;
  for (int j = 0; j < R; ++j) for (int i = 0; i < R; ++i) {
   const int x = left ? ox + i : ox - 1 - i, y = top ? oy + j : oy - 1 - j;
   const float dx = x + .5f - cx, dy = y + .5f - cy;
   if (x >= 0 && x < w && y >= 0 && y < h && dx * dx + dy * dy > static_cast<float>(R * R)) out[y * w + x] = 0;
  }
 }
 // diagonal strokes (polygons in the glyph box)
 float q[2][8]; int polys = 0;
 const float W = static_cast<float>(w), H = static_cast<float>(h), V = static_cast<float>(v), HZ = static_cast<float>(hz);
 if (c == '7') { const float p[8] = {W - V * 1.05f, HZ, W, HZ, W * .2f + V, H, W * .2f, H}; for (int i = 0; i < 8; ++i) q[0][i] = p[i]; polys = 1; }
 else if (c == 'N') { const float p[8] = {0, 0, V * 1.15f, 0, W, H, W - V * 1.15f, H}; for (int i = 0; i < 8; ++i) q[0][i] = p[i]; polys = 1; }
 else if (c == 'M') {
  const float a[8] = {0, 0, V, 0, W / 2 + V * .45f, H * .58f, W / 2 - V * .45f, H * .58f};
  const float b[8] = {W - V, 0, W, 0, W / 2 + V * .45f, H * .58f, W / 2 - V * .45f, H * .58f};
  for (int i = 0; i < 8; ++i) { q[0][i] = a[i]; q[1][i] = b[i]; } polys = 2;
 } else if (c == 'K') {
  const float a[8] = {V * .7f, H * .5f, W - V * 1.1f, 0, W, 0, V * 1.5f, H * .62f};
  const float b[8] = {V * 1.3f, H * .44f, W, H, W - V * 1.1f, H, V * .7f, H * .6f};
  for (int i = 0; i < 8; ++i) { q[0][i] = a[i]; q[1][i] = b[i]; } polys = 2;
 }
 for (int p = 0; p < polys; ++p) for (int y = 0; y < h; ++y) for (int x = 0; x < w; ++x) if (inPoly(x + .5f, y + .5f, q[p], 4)) out[y * w + x] = 1;
 return w;
}

// One size of the face: glyphs for the printable ASCII range, built on first use into a byte pool.
struct Face { uint8_t h, w, gap; uint8_t width[64]; uint32_t off[64]; };
struct Pool { uint8_t *data; uint32_t used, cap; };

inline void face(Face &f, int h, int w, int gap) {
 f.h = static_cast<uint8_t>(h); f.w = static_cast<uint8_t>(w); f.gap = static_cast<uint8_t>(gap);
 for (int i = 0; i < 64; ++i) { f.width[i] = 0; f.off[i] = 0xffffffffu; }
}
inline Mask get(Face &f, Pool &pool, char c) {
 if (c >= 'a' && c <= 'z') c = static_cast<char>(c - 32);
 const int i = (c < 32 || c > 95) ? 0 : c - 32;
 if (f.off[i] == 0xffffffffu) {
  const uint32_t need = static_cast<uint32_t>(f.w) * f.h;
  if (pool.used + need > pool.cap) return Mask{0, f.h, nullptr};
  f.off[i] = pool.used; f.width[i] = static_cast<uint8_t>(build(c, f.w, f.h, pool.data + pool.used));
  pool.used += need;
 }
 return Mask{f.width[i], f.h, pool.data + f.off[i]};
}
inline int width(Face &f, Pool &pool, const char *s) {
 int w = 0, n = 0;
 for (; s[n]; ++n) w += get(f, pool, s[n]).w + (n ? f.gap : 0);
 return w;
}

// A clipped RGB565 target.
struct Target { uint16_t *px; uint32_t stride; int x0, y0, x1, y1; };
inline void clip(Target &t, int x, int y, int w, int h) { t.x0 = x; t.y0 = y; t.x1 = x + w; t.y1 = y + h; }
inline void noclip(Target &t) { t.x0 = 0; t.y0 = 0; t.x1 = 320; t.y1 = 172; }
inline void rect(Target &t, int x, int y, int w, int h, uint16_t c) {
 const int xa = x < t.x0 ? t.x0 : x, xb = x + w > t.x1 ? t.x1 : x + w, ya = y < t.y0 ? t.y0 : y, yb = y + h > t.y1 ? t.y1 : y + h;
 for (int yy = ya; yy < yb; ++yy) { uint16_t *row = t.px + yy * t.stride; for (int xx = xa; xx < xb; ++xx) row[xx] = c; }
}
inline void frame(Target &t, int x, int y, int w, int h, uint16_t c) {
 rect(t, x, y, w, 1, c); rect(t, x, y + h - 1, w, 1, c); rect(t, x, y, 1, h, c); rect(t, x + w - 1, y, 1, h, c);
}
inline void stamp(Target &t, const Mask &m, int x, int y, uint16_t c) {
 if (!m.m) return;
 for (int j = 0; j < m.h; ++j) {
  const int Y = y + j; if (Y < t.y0 || Y >= t.y1) continue;
  uint16_t *row = t.px + Y * t.stride; const uint8_t *src = m.m + j * m.w;   // masks are stored at their own width
  for (int i = 0; i < m.w; ++i) { const int X = x + i; if (src[i] && X >= t.x0 && X < t.x1) row[X] = c; }
 }
}
// a word left to right; returns its width
inline int text(Target &t, Face &f, Pool &pool, const char *s, int x, int y, uint16_t c) {
 const int x0 = x;
 for (int n = 0; s[n]; ++n) { const Mask m = get(f, pool, s[n]); stamp(t, m, x, y, c); x += m.w + f.gap; }
 return x - x0 - f.gap;
}
// reading upward: the word's left edge sits at y0 (bottom), the glyph tops face x0
inline void textUp(Target &t, Face &f, Pool &pool, const char *s, int x0, int y0, uint16_t c) {
 int along = 0;
 for (int n = 0; s[n]; ++n) {
  const Mask m = get(f, pool, s[n]);
  if (m.m) for (int j = 0; j < m.h; ++j) for (int i = 0; i < m.w; ++i) if (m.m[j * m.w + i]) {
   const int X = x0 + j, Y = y0 - (along + i);
   if (X >= t.x0 && X < t.x1 && Y >= t.y0 && Y < t.y1) t.px[Y * t.stride + X] = c;
  }
  along += m.w + f.gap;
 }
}

// The 5 x 7 pixel font for the two small labels (LAYERS, BAR).
inline void tiny(Target &t, const char *s, int x, int y, uint16_t c) {
 struct G { char ch; const char *r[7]; };
 static const G g[] = {
  {'L', {"#...", "#...", "#...", "#...", "#...", "#...", "####"}}, {'A', {".###.", "#...#", "#...#", "#####", "#...#", "#...#", "#...#"}},
  {'Y', {"#...#", "#...#", ".#.#.", "..#..", "..#..", "..#..", "..#.."}}, {'E', {"#####", "#....", "#....", "####.", "#....", "#....", "#####"}},
  {'R', {"####.", "#...#", "#...#", "####.", "#.#..", "#..#.", "#...#"}}, {'S', {".####", "#....", "#....", ".###.", "....#", "....#", "####."}},
  {'B', {"####.", "#...#", "#...#", "####.", "#...#", "#...#", "####."}},
 };
 for (int n = 0; s[n]; ++n) {
  for (const G &q : g) if (q.ch == s[n]) {
   int w = 0; while (q.r[0][w]) ++w;
   for (int j = 0; j < 7; ++j) for (int i = 0; i < w; ++i) if (q.r[j][i] == '#') rect(t, x + i, y + j, 1, 1, c);
   x += w + 1; break;
  }
 }
}
}  // namespace type
