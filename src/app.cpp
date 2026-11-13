// Hype Dial — one dial from chill to chaos, in brutalist kinetic type. Original MIT source; all hardware access
// uses the pinned public SDK.
//
// The screen follows the booth concept (artifacts/resona_booth/concepts/hype-dial.js): an 8 px grid, a genre
// accent column with the genre and BPM reading upward, a giant block numeral for the energy, the layer roster,
// energy ticks, beat squares and a bar count. Long-press the big knob to BUILD (countdown, speed lines, shake), then
// click it to schedule DROP on the next bar; the top knob flips genre through a departure board; hold the top knob and
// turn the big one for the DJ filter. Press the big knob and turn it to SCRATCH the last beat; while building, turn the
// top knob to set the ROLL; hold top + click big for KICK KILL; double-click the top knob for a FILL; at energy 8+ a
// hard pick fires the chord stab. Holding a knob shows what the other does. Flat fills and 1-bit masks only. The beat clock runs on the Hype Engine's
// own step lengths, so kicks, flashes and bars line up with the audio.
//
// The Web, Mobile and MIDI controllers (controller/, CCs on channel 1, see README.md) reach energy, genre, tune, kick
// kill, the DJ filter, the build's roll, LEVEL and BACKING (both saved), show what the App is doing (PLAY / BUILD / DROP / STOP), and
// play four pads: BUILD (hold, let go to drop), STUTTER, FILL and STOP (hold for a tape stop, let go to restart).
#include <resona/platform.h>
#include "own_audio.h"
#include <resona/ui.h>
#include <resona/rgb.h>
#include <resona/math.h>
#include "demo_capture.h"
#include "trip_guard.h"
#include <resona/startup_helpers.h>
#include "splash_data.h"
#include "type.h"
#include "controller_link.h"
namespace {
const resona_platform *P;
#define C565(hex) RESONA_RGB(((hex) >> 16) & 255, ((hex) >> 8) & 255, (hex) & 255)
constexpr uint16_t BG = C565(0x0A0A0A), GRID = C565(0x1A1A19), WHITE = C565(0xF2F2EE), CONC = C565(0x8A8A86),
                   DARK = C565(0x3A3A38), INK = C565(0x0A0A0A);
struct Genre { const char *name, *bpm; uint32_t acc, stepSamples; };
// Step lengths match the engine at 48 kHz.
const Genre kGenres[6] = {{"HOUSE", "124", 0xE4FF1A, 5806}, {"TECHNO", "130", 0xFF5A1F, 5538},
                          {"HIP-HOP", "90", 0xFF2A3D, 8000}, {"DNB", "172", 0x19E3FF, 4186},
                          {"FUNK", "110", 0x69E84A, 6545}, {"LATIN", "108", 0xC58AFF, 6667}};
const char *const kLayers[7] = {"PAD", "HATS", "KICK", "BASS", "CLAP", "ARP", "STAB"};
const int kOrder[7] = {0, 1, 0, 4, 6, 2, 9};              // pad/pulse at 0, hats at 1, melody at 2
enum { ENERGY, GENRE, BUILD, STUTTER, STOP, TUNE, LEVEL, FILTER, SCRATCH, ROLL, KILL, FILL, BACKING, PARAMS };
const char *const kRolls[4] = {"1/4", "1/8", "1/16", "1/32"};

constexpr uint32_t kPoolBytes = 200 * 1024;
struct Heap {
 uint16_t grid[320 * 172];
 type::Face num, w40, w20, w16, w12, m10, m8;
 type::Pool pool; uint8_t data[kPoolBytes];
};
struct Line { float x, v; int16_t y, len, th, white; };
struct Saved { uint32_t magic; int level; };
constexpr uint32_t kMagic = 0x48595031u;   // "HYP1"
struct BackingSaved { uint32_t magic; int backing; };
constexpr uint32_t kBackingMagic = 0x48594231u; // "HYB1"; separate slot preserves old LEVEL settings
// The controllers' MIDI map: CC on channel 1 (controller/shared/profile.js says the same).
enum { CC_ENERGY = 20, CC_GENRE, CC_TUNE, CC_KILL, CC_FILTER, CC_ROLL, CC_LEVEL, CC_STATUS, CC_BACKING,
       CC_BUILD = 30, CC_STUTTER, CC_FILL, CC_STOP };
struct Mirror { int energy, genre, tune, kill, filter, roll, status; };   // the controllers' view of the state
struct State {
 Heap *h;
 int energy, genre, pendingGenre, tune, stutter, backing, building, buildRelease, buildStep, dropStep, dropShow, slam, rainbowUntil;
 int step, bar, bigDown, bigTurned, suppressBig, rawBigSeen, topDown, chordUsed, stopped, attract, boardFrame, boardDir, notice;
 int ready, missing, error, gtrJolt; float filter, t, kickFlash, gtr; int32_t brake, bothAt;
 uint32_t ms, bigDownAt, idle, nextTry, noticeUntil, seed, lampKey; uint8_t flash[7], in, out, cpu;
 Line lines[18]; int nLines;
 uint32_t stDraws, stDrawSum, stDrawMax, stSlow, stFrames, stFrameMax, lastFrameAt, stResetAt;   // bench stats
 int scratch, scratchPos, rollIdx, kill, fillToken, fillPending, fillBar, filterTouched, lastIn; float scratchX;
 uint32_t topDownAt, stabUntil; trip_guard::Guard trip;
 Saved sv; uint32_t saveAt; Mirror c; ctl::Link link;
};
int clampi(int v, int lo, int hi) { return v < lo ? lo : (v > hi ? hi : v); }
float stepMs(const State *s) { return kGenres[s->genre].stepSamples / 48.f; }
uint32_t rnd(State *s) { s->seed ^= s->seed << 13; s->seed ^= s->seed >> 17; s->seed ^= s->seed << 5; return s->seed; }
float rnd01(State *s) { return (rnd(s) >> 8) * (1.f / 16777216.f); }

/* ---- the engine -------------------------------------------------------------------------------------------- */
void failed(State *s, int e) { if (e < 0) s->error = e; }
// The App owns audio: Guitar -> Hype Engine -> Output (own_audio.h). Created by package id, retried until installed.
own_audio::Graph g_audio;
bool voice(State *s) {
 const int c = own_audio::start(P, g_audio, "org.resona.demo.hype-engine", "0.3.12", 0);
 if (c < 0) { s->ready = 0; s->missing = c == RESONA_ERR_PACK_ID; s->error = c == RESONA_ERR_PACK_ID ? 0 : c; return false; }
 resona_effect_info fi = {}; const int r = P->effect_info(g_audio.h[0], &fi);
 const bool ok = r == RESONA_OK && fi.param_count == PARAMS;
 if (!ok) { s->ready = 0; s->missing = 0; s->error = r < 0 ? r : RESONA_ERR_STATE; own_audio::stop(P, g_audio); }
 return ok;
}
void param(State *s, int n, float v) { if (s->ready) failed(s, P->effect_param_set(g_audio.h[0], static_cast<uint32_t>(n), v)); }
void push(State *s) {
 param(s, ENERGY, static_cast<float>(s->energy)); param(s, GENRE, static_cast<float>(s->pendingGenre >= 0 ? s->pendingGenre : s->genre));
 param(s, BUILD, s->building ? 1.f : 0.f); param(s, STUTTER, static_cast<float>(s->stutter)); param(s, STOP, s->stopped || s->brake >= 0 ? 1.f : 0.f);
 param(s, TUNE, static_cast<float>(s->tune)); param(s, LEVEL, static_cast<float>(s->sv.level)); param(s, FILTER, s->filter * 100);
 param(s, SCRATCH, s->scratch ? static_cast<float>(s->scratchPos + 17) : 0.f); param(s, ROLL, s->rollIdx >= 0 ? static_cast<float>(s->rollIdx + 1) : 0.f);
 param(s, KILL, s->kill ? 1.f : 0.f); param(s, FILL, static_cast<float>(s->fillToken)); param(s, BACKING, static_cast<float>(s->backing));
}
// The OS bypassed the engine (CPU time guard or a fault): drop the graph, wait out the back-off, then rebuild it.
void trip(State *s) { s->ready = 0; s->error = RESONA_ERR_BUDGET; s->nextTry = s->ms + trip_guard::tripped(s->trip, s->ms); own_audio::stop(P, g_audio); }
// Once a second: create the engine in the App graph, enable it and send the controls; while it runs, watch for a fault.
void bootstrap(State *s) {
 if (s->ms < s->nextTry) return;
 s->nextTry = s->ms + 1000;
 const bool was = s->ready != 0;
 if (was && trip_guard::faulted(P, g_audio.h[0])) { trip(s); return; }
 if (!voice(s) || was) return;
 const int r = P->effect_enable(g_audio.h[0], 1); if (r < 0) { failed(s, r); own_audio::stop(P, g_audio); return; }
 s->ready = 1; s->missing = 0; s->error = 0; push(s);
}

/* ---- transport: a mirror of the engine's sequencer ------------------------------------------------------------- */
void drop(State *s, int st) {
 s->building = 0; s->buildRelease = 0; s->energy = 10; s->dropStep = st; s->dropShow = 1; s->rainbowUntil = st + 16; s->slam = -1;
 if (s->rollIdx >= 0) { s->rollIdx = -1; param(s, ROLL, 0); }
 param(s, ENERGY, 10);
}
void onStep(State *s, int st) {
 const int k = st & 15, bar = st >> 4;
 if (k == 0) {
  s->bar = bar % 99 + 1;
  if (s->pendingGenre >= 0) {            // the engine commits the genre on its bar; rebase our clock to its step length
   const float frac = s->t - static_cast<float>(st) * stepMs(s);
   s->genre = s->pendingGenre; s->pendingGenre = -1; s->boardFrame = -1;
   s->t = static_cast<float>(st) * stepMs(s) + frac;
  }
  if (s->buildRelease) drop(s, st);
  if (s->fillPending) { s->fillPending = 0; s->fillBar = bar; }     // the engine plays its fill on this bar
 }
 if (s->stopped || s->brake >= 0) return;
 const int e = s->energy, g = s->genre;
 const bool kick = g == 2 ? (k == 0 || k == 7 || k == 10) :
                   g == 3 ? (k == 0 || k == 6 || k == 10) :
                   g == 4 ? (k == 0 || k == 3 || k == 8 || k == 11) :
                   g == 5 ? (k == 0 || k == 6 || k == 10 || k == 14) : (k % 4 == 0);
 if (kick && e >= 2 && !s->kill) { s->kickFlash = 1; s->flash[2] = 2; }
 if (e >= 2 && (g == 1 || g == 3 || k % 2 == 0 || (g == 2 && (k == 7 || k == 15)))) s->flash[1] = 2;
 const bool bass = g == 1 ? (k == 0 || k == 8 || (e >= 4 && k % 4 == 0)) :
                   g == 2 ? (k == 0 || k == 7 || k == 10 || (e >= 4 && k == 14)) :
                   g == 4 ? (k == 0 || k == 3 || k == 6 || k == 10 || k == 13) :
                   g == 5 ? (k == 0 || k == 6 || k == 10 || k == 14) : (k % 2 == 0);
 if (e >= (g == 1 || g == 2 || g == 4 || g == 5 ? 2 : 4) && bass) s->flash[3] = 2;
 if ((g == 5 && e >= 2 && (k == 0 || k == 3 || k == 6 || k == 10 || k == 12)) ||
     (g != 5 && (k == 4 || k == 12) && (e >= 6 || (g >= 2 && e >= 2)))) s->flash[4] = 2;
 if (e >= 8 || (g == 3 && e >= 2 && (k == 0 || k == 3 || k == 6 || k == 10 || k == 13))) s->flash[5] = 2;
 if (e >= 9 && k % 4 == 2) s->flash[6] = 2;
 if (e >= 1 && k == 0) s->flash[0] = 2;
}

/* ---- input ----------------------------------------------------------------------------------------------------- */
void wake(State *s) { s->idle = 0; s->attract = 0; }
void restart(State *s) { s->stopped = 0; s->brake = -1; param(s, STOP, 0); }
void reset(State *s) {
 s->genre = 0; s->pendingGenre = -1; s->tune = 0; s->energy = 2; s->building = s->buildRelease = 0; s->filter = 0;
 s->stopped = 0; s->brake = -1; s->dropShow = 0; s->slam = -1; s->rainbowUntil = -1; s->boardFrame = -1; s->attract = 1; s->idle = 0;
 s->scratch = 0; s->scratchPos = 0; s->rollIdx = -1; s->kill = 0; s->fillPending = 0; s->fillBar = -1;
 push(s);
}
void say(State *s, int what) { s->notice = what; s->noticeUntil = s->ms + 700; }
void turn(State *s, int enc, int delta) {
 if (s->stopped || s->brake >= 0) { restart(s); wake(s); return; }
 wake(s);
 if (enc == 1) {
  s->bigTurned = 1;
  if (s->bigDown && !s->topDown && !s->building) {          // press + turn: SCRATCH the last beat
   s->scratch = 1; s->scratchPos = clampi(s->scratchPos + delta, -16, 16);
   const float x = s->scratchX + delta * 9.f; s->scratchX = x < -36 ? -36 : (x > 36 ? 36 : x);
   param(s, SCRATCH, static_cast<float>(s->scratchPos + 17)); return;
  }
  if (s->topDown) {                                        // hold top + turn big: the DJ filter
   s->chordUsed = 1; s->filterTouched = 1;
   float f = s->filter + delta * .05f; f = f < -1 ? -1 : (f > 1 ? 1 : f); s->filter = static_cast<float>(type::rnd(f * 20)) / 20;
   param(s, FILTER, s->filter * 100); return;
  }
  if (s->building) return;
  const int e = clampi(s->energy + delta, 0, 10);
  if (e != s->energy) { s->energy = e; param(s, ENERGY, static_cast<float>(e)); }
 } else {
  if (s->building || (s->bigDown && !s->topDown && !s->scratch)) {   // while building: the ROLL rate
   if (!s->building) { s->building = 1; s->buildRelease = 0; s->buildStep = s->step + 1; s->nLines = 0; s->suppressBig = 1; param(s, BUILD, 1); }
   s->rollIdx = clampi((s->rollIdx < 0 ? 1 : s->rollIdx) + (delta > 0 ? 1 : -1), 0, 3);
   param(s, ROLL, static_cast<float>(s->rollIdx + 1)); return;
  }
  const int base = s->pendingGenre >= 0 ? s->pendingGenre : s->genre, n = ((base + delta) % 6 + 6) % 6;
  s->boardDir = delta > 0 ? 1 : -1; s->pendingGenre = n == s->genre ? -1 : n; s->boardFrame = s->pendingGenre >= 0 ? 0 : -1;
  param(s, GENRE, static_cast<float>(n));
 }
}
void click(State *s, int enc) {
 if (s->stopped || s->brake >= 0) { restart(s); wake(s); return; }
 if (enc == 0) {
  if (s->chordUsed) { s->chordUsed = 0; return; }
  if (s->filter != 0) { s->filter = 0; param(s, FILTER, 0); return; }
  wake(s); s->tune = (s->tune + 1) & 7; param(s, TUNE, static_cast<float>(s->tune)); say(s, 2);
 } else {
  // Box firmware can deliver CLICK/LONG without raw key edges. In that case
  // bigTurned never resets, so use the gesture itself as the complete action.
  if (!s->rawBigSeen) {
   if (s->building) {
    if (!s->buildRelease) { s->buildRelease = 1; param(s, BUILD, 0); }
   } else { wake(s); s->stutter = (s->stutter + 1) & 127; param(s, STUTTER, static_cast<float>(s->stutter)); say(s, 1); }
   return;
  }
  if (s->bigTurned || s->suppressBig || s->building) return;
  if (s->topDown) { s->kill = !s->kill; s->chordUsed = 1; param(s, KILL, s->kill ? 1.f : 0.f); wake(s); return; }   // KICK KILL
  if (s->chordUsed) return;
  wake(s); s->stutter = (s->stutter + 1) & 127; param(s, STUTTER, static_cast<float>(s->stutter)); say(s, 1);
 }
}
void doubleClick(State *s, int enc) {
 if (enc != 0 || s->stopped || s->brake >= 0) return;
 wake(s); s->fillToken = (s->fillToken + 1) & 127; s->fillPending = 1; param(s, FILL, static_cast<float>(s->fillToken));   // FILL next bar
}
void startBuild(State *s);
void longPress(State *s, int enc) {
 if (enc == 1 && !s->rawBigSeen) {
  if (!s->building && !s->stopped && s->brake < 0 && !s->dropShow) startBuild(s);
  return;
 }
 if (enc != 0 || s->bigDown || s->chordUsed || (s->filter != 0 && s->topDown) || s->stopped) return;
 wake(s); s->brake = 0; param(s, STOP, 1);                // tape stop
}
void key(State *s, int enc, bool down) {
 if (enc == 1) {
  s->rawBigSeen = 1;
  if (down) { s->bigDown = 1; s->bigDownAt = s->ms; s->bigTurned = 0; s->suppressBig = 0; }
  else {
   s->bigDown = 0;
   if (s->scratch) { s->scratch = 0; s->scratchPos = 0; param(s, SCRATCH, 0); }          // let go: back on the beat
   else if (s->building && !s->buildRelease) { s->buildRelease = 1; param(s, BUILD, 0); }
  }
 } else { s->topDown = down; if (down) { s->chordUsed = 0; s->filterTouched = 0; s->topDownAt = s->ms; } }
 // stop / start only when both go down together; hold top then click big is the kick kill
 const uint32_t apart = s->bigDownAt > s->topDownAt ? s->bigDownAt - s->topDownAt : s->topDownAt - s->bigDownAt;
 if (s->topDown && s->bigDown) { if (s->bothAt < 0 && apart < 150) s->bothAt = static_cast<int32_t>(s->ms); }
 else if (s->bothAt >= 0) {                                // both pressed then released: stop / start
  const uint32_t held = s->ms - static_cast<uint32_t>(s->bothAt); s->bothAt = -1;
  if (held < 3000) { if (s->stopped) restart(s); else { s->stopped = 1; param(s, STOP, 1); } }
  s->chordUsed = 1; s->bigTurned = 1; s->suppressBig = 1;
 }
}

/* ---- the controllers ------------------------------------------------------------------------------------------ */
const int kBinds = 13;
ctl::Bind kBind[kBinds];
void bind(State *s) {
 Mirror &c = s->c;
 const ctl::Bind b[kBinds] = {
  {CC_ENERGY, ctl::RANGE, 0, 10, &c.energy}, {CC_GENRE, ctl::STEPS, 0, 5, &c.genre}, {CC_TUNE, ctl::STEPS, 0, 7, &c.tune},
  {CC_KILL, ctl::STEPS, 0, 1, &c.kill}, {CC_FILTER, ctl::RANGE, -64, 63, &c.filter}, {CC_ROLL, ctl::STEPS, 0, 4, &c.roll},
  {CC_LEVEL, ctl::RANGE, 0, 100, &s->sv.level}, {CC_STATUS, ctl::READ_STEPS, 0, 3, &c.status},
  {CC_BACKING, ctl::RANGE, 0, 150, &s->backing},
  {CC_BUILD, ctl::TRIGGER, 0, 1, nullptr}, {CC_STUTTER, ctl::TRIGGER, 0, 1, nullptr}, {CC_FILL, ctl::TRIGGER, 0, 1, nullptr}, {CC_STOP, ctl::TRIGGER, 0, 1, nullptr}};
 for (int i = 0; i < kBinds; ++i) kBind[i] = b[i];
}
// what the App does now, as the controllers see it; refreshed before every sync, so knob moves are reported
void mirror(State *s) {
 Mirror &c = s->c; c.energy = s->energy; c.genre = s->pendingGenre >= 0 ? s->pendingGenre : s->genre; c.tune = s->tune; c.kill = s->kill;
 c.filter = type::rnd(s->filter * (s->filter > 0 ? 63 : 64));   // -64..63: one CC step each; CC 64 is clean
 c.roll = s->rollIdx + 1;
 c.status = s->stopped || s->brake >= 0 ? 3 : s->building ? 1 : (s->dropShow || s->rainbowUntil > s->step) ? 2 : 0;
}
void startBuild(State *s) { s->building = 1; s->buildRelease = 0; s->buildStep = s->step + 1; s->nLines = 0; s->suppressBig = 1; wake(s); param(s, BUILD, 1); }
// a controller moved a value or hit a pad: the same as the knob gesture that does it
void apply(State *s, const ctl::Msg &m) {
 const Mirror &c = s->c; const bool halted = s->stopped || s->brake >= 0;
 switch (m.cc) {
 case CC_ENERGY: if (halted) restart(s); wake(s); if (!s->building) { s->energy = c.energy; param(s, ENERGY, static_cast<float>(s->energy)); } break;
 case CC_GENRE: wake(s); s->boardDir = c.genre >= (s->pendingGenre >= 0 ? s->pendingGenre : s->genre) ? 1 : -1;
  s->pendingGenre = c.genre == s->genre ? -1 : c.genre; s->boardFrame = s->pendingGenre >= 0 ? 0 : -1; param(s, GENRE, static_cast<float>(c.genre)); break;
 case CC_TUNE: wake(s); s->tune = c.tune; param(s, TUNE, static_cast<float>(s->tune)); say(s, 2); break;
 case CC_KILL: wake(s); s->kill = c.kill; param(s, KILL, s->kill ? 1.f : 0.f); break;
 case CC_FILTER: wake(s); s->filter = static_cast<float>(c.filter) / (c.filter > 0 ? 63 : 64); param(s, FILTER, s->filter * 100); break;
 case CC_ROLL: if (s->building) { s->rollIdx = c.roll - 1; param(s, ROLL, static_cast<float>(c.roll)); } break;   // a roll is for a build
 case CC_LEVEL: param(s, LEVEL, static_cast<float>(s->sv.level)); s->saveAt = s->ms + 1200; break;
 case CC_BACKING: param(s, BACKING, static_cast<float>(s->backing)); s->saveAt = s->ms + 1200; break;
 case CC_BUILD:
  if (m.down && !s->building && !halted && !s->dropShow) startBuild(s);
  else if (!m.down && s->building && !s->buildRelease) { s->buildRelease = 1; param(s, BUILD, 0); }
  break;
 case CC_STUTTER: if (!m.down) break; if (halted) { restart(s); wake(s); break; }
  wake(s); s->stutter = (s->stutter + 1) & 127; param(s, STUTTER, static_cast<float>(s->stutter)); say(s, 1); break;
 case CC_FILL: if (m.down) doubleClick(s, 0); break;
 case CC_STOP:
  if (m.down && !halted) { wake(s); s->brake = 0; param(s, STOP, 1); }   // tape stop
  else if (!m.down && halted) { restart(s); wake(s); }
  break;
 }
}

/* ---- drawing --------------------------------------------------------------------------------------------------- */
uint16_t acc(const State *s) { return C565(kGenres[s->genre].acc); }
void banner(State *s, type::Target &T, const char *label) {
 Heap &h = *s->h;
 type::rect(T, 0, 0, 24, 172, acc(s)); type::rect(T, 24, 0, 2, 172, CONC);
 type::textUp(T, h.w16, h.pool, label, 4, 160, INK);
 type::textUp(T, h.w12, h.pool, kGenres[s->genre].bpm, 6, 40, INK);
}
void number(State *s, type::Target &T, int n, uint16_t color, bool lead, int dy, int dx) {
 Heap &h = *s->h; n = clampi(n, 0, 99);
 const char a = static_cast<char>('0' + n / 10), b = static_cast<char>('0' + n % 10);
 type::stamp(T, type::get(h.num, h.pool, a), 34 + dx, 6 + dy, lead && a == '0' ? CONC : color);
 type::stamp(T, type::get(h.num, h.pool, b), 114 + dx, 6 + dy, color);
}
void ticks(State *s, type::Target &T, int e) {
 for (int i = 0; i <= 10; ++i) {
  const int x = 34 + 14 * i;
  if (i < e) type::rect(T, x, 160, 12, 10, WHITE); else if (i == e) type::rect(T, x, 160, 12, 10, acc(s)); else type::frame(T, x, 160, 12, 10, DARK);
 }
}
void roster(State *s, type::Target &T, bool all, float hollow) {
 Heap &h = *s->h; const uint16_t a = acc(s);
 type::tiny(T, "LAYERS", 196, 4, CONC);
 for (int i = 0; i < 7; ++i) {
  const int y = 16 + 18 * i; const bool on = all || s->energy >= kOrder[i] ||
      (i == 4 && s->energy >= 2 && s->genre >= 2) || (i == 4 && s->energy >= 5 && s->genre == 0) ||
      (i == 3 && s->energy >= 2 && (s->genre == 1 || s->genre == 2 || s->genre == 4 || s->genre == 5)) ||
      (i == 5 && s->energy >= 2 && s->genre == 3);
  type::text(T, h.m10, h.pool, i == 4 && s->genre == 5 ? "CLAVE" : kLayers[i], 196, y + 1, WHITE);
  const bool fillNow = s->fillBar >= 0 && s->fillBar == (s->step < 0 ? 0 : s->step >> 4);
  if (!all && i == 2 && s->kill) { type::frame(T, 240, y, 76, 12, a); type::text(T, h.m8, h.pool, "KILL", 244, y + 2, a); continue; }
  if (!all && i == 2 && fillNow) { type::rect(T, 240, y, 76, 12, (s->ms / 120) % 2 ? WHITE : a); type::text(T, h.m8, h.pool, "FILL", 244, y + 2, INK); continue; }
  if (!all && i == 6 && s->ms < s->stabUntil) { type::rect(T, 240, y, 76, 12, WHITE); type::text(T, h.m8, h.pool, "GTR", 244, y + 2, INK); continue; }
  if (all && (i == 0 || i == 5 || i == 6)) {               // PAD, ARP, STAB hollow out through a build
   const int fillH = type::rnd(12 * (1 - hollow));
   type::frame(T, 240, y, 76, 12, a); if (fillH > 0) type::rect(T, 240, y + 12 - fillH, 76, fillH, a);
  } else if (on) type::rect(T, 240, y, 76, 12, s->flash[i] && !all ? WHITE : a);
  else type::frame(T, 240, y, 76, 12, DARK);
  if (all && i == 2) { char r[12]; P->format(r, sizeof r, s->rollIdx >= 0 ? "ROLL %s" : "ROLL", s->rollIdx >= 0 ? kRolls[s->rollIdx] : ""); type::text(T, h.m8, h.pool, r, 244, y + 2, INK); }
 }
}
void beats(State *s, type::Target &T) {
 Heap &h = *s->h; const int beat = (s->step < 0 ? 0 : s->step & 15) >> 2;
 for (int i = 0; i < 4; ++i) { const int x = 196 + 18 * i; if (i == beat && !s->stopped) type::rect(T, x, 146, 14, 12, acc(s)); else type::frame(T, x, 146, 14, 12, DARK); }
 type::tiny(T, "BAR", 274, 146, CONC);
 char b[3] = {static_cast<char>('0' + s->bar / 10), static_cast<char>('0' + s->bar % 10), 0};
 type::text(T, h.w12, h.pool, b, 298, 146, WHITE);
}
void tri(type::Target &T, int x, int y, uint16_t c) { for (int j = 0; j < 4; ++j) type::rect(T, x + j, y + j, 7 - 2 * j, 1, c); }
const char *gtrLabel(const State *s) {
 const int e = s->energy;                                  // what the engine does to the guitar at this energy
 if (e <= 1) return "GTR CLEAN";
 if (e <= 4) {
  static const char *const label[6] = {"GTR CLEAN", "GTR GRIT", "GTR WARM", "GTR CHOP", "GTR SNAP", "GTR LATIN"};
  return label[s->genre];
 }
 return e <= 6 ? "GTR CHOP 1/8" : "GTR CHOP 1/16";
}
void hint(State *s, type::Target &T) {
 Heap &h = *s->h;
 if (s->ms < s->noticeUntil) {
  char b[12]; const char *t = "STUTTER";
  if (s->notice == 2) { P->format(b, sizeof b, "TUNE %d", s->tune + 1); t = b; }
  type::text(T, h.m8, h.pool, t, 196, 162, acc(s)); return;
 }
 if (s->scratch) { tri(T, 196, 163, acc(s)); type::text(T, h.m8, h.pool, "LET GO = ON BEAT", 206, 162, acc(s)); return; }
 if (s->ms < s->stabUntil) { type::text(T, h.m8, h.pool, "GTR STAB", 196, 162, acc(s)); return; }
 if (s->attract) { if ((s->ms / 500) % 2 == 0) { tri(T, 196, 163, WHITE); type::text(T, h.m8, h.pool, "TURN BIG KNOB", 206, 162, WHITE); } return; }
 if (s->gtr > .02f) { type::text(T, h.m8, h.pool, gtrLabel(s), 196, 162, acc(s)); return; }
 tri(T, 196, 163, CONC); type::text(T, h.m8, h.pool, "HOLD = BUILD", 206, 162, CONC);
}
void restoreGrid(State *s, type::Target &T, int x, int y, int w, int hh) {
 for (int j = y; j < y + hh; ++j) for (int i = x; i < x + w; ++i) T.px[j * T.stride + i] = s->h->grid[j * 320 + i];
}
void filterTag(State *s, type::Target &T, int x, int y, int size) {
 Heap &h = *s->h; char b[12];
 if (s->filter == 0) P->format(b, sizeof b, "CLEAN"); else P->format(b, sizeof b, "%s %d%%", s->filter < 0 ? "LP" : "HP", type::rnd((s->filter < 0 ? -s->filter : s->filter) * 100));
 type::Face &f = size == 10 ? h.m10 : h.m8; const int w = type::width(f, h.pool, b);
 type::rect(T, x, y, w + (size == 10 ? 6 : 4), size == 10 ? 13 : 11, acc(s)); type::text(T, f, h.pool, b, x + (size == 10 ? 3 : 2), y + (size == 10 ? 2 : 1), INK);
}
void filterStrip(State *s, type::Target &T) {
 Heap &h = *s->h;
 restoreGrid(s, T, 26, 157, 294, 15);
 type::text(T, h.m8, h.pool, "LP", 34, 161, WHITE); type::text(T, h.m8, h.pool, "HP", 302, 161, WHITE);
 type::rect(T, 50, 164, 120, 2, WHITE); type::rect(T, 176, 164, 120, 2, WHITE);
 type::rect(T, type::rnd(173 + s->filter * 120) - 6, 159, 12, 12, acc(s));
 restoreGrid(s, T, 196, 144, 124, 14);
 filterTag(s, T, 196, 145, 10);
}
void board(State *s, type::Target &T) {
 Heap &h = *s->h;
 const float slide = s->boardFrame / 4.f < 1 ? s->boardFrame / 4.f : 1;
 const int to = s->pendingGenre, off = type::rnd((1 - slide) * 80 * s->boardDir);
 type::rect(T, 26, 0, 294, 172, BG);
 for (int k = -2; k <= 2; ++k) {
  const int gi = ((to + k) % 6 + 6) % 6, y = 64 + k * 80 + off; const Genre &g = kGenres[gi]; const bool cur = k == 0;
  if (y > 172 || y + 44 < 0) continue;
  if (cur) type::rect(T, 26, y, 294, 44, C565(g.acc));
  type::text(T, h.w40, h.pool, g.name, 34, y + 2, cur ? INK : CONC);
  const int bw = type::width(h.w20, h.pool, g.bpm); type::text(T, h.w20, h.pool, g.bpm, 310 - bw, y + 12, cur ? INK : CONC);
  type::rect(T, 26, y + 44 + 17, 294, 2, CONC);
 }
}
void problem(State *s, type::Target &T) {
 if (!s->missing && !s->error) return;
 Heap &h = *s->h; char b[32];
 if (s->error == RESONA_ERR_BUDGET) P->format(b, sizeof b, "CPU LIMIT"); else if (s->error) P->format(b, sizeof b, "AUDIO ERROR %d", s->error); else P->format(b, sizeof b, "INSTALL HYPE ENGINE IN LAB");
 type::rect(T, 26, 158, 294, 14, BG); type::text(T, h.m8, h.pool, b, 34, 162, WHITE);
}
void lamp(State *s, float k, uint32_t rgb) {
 if (P->version < 5 || P->size < offsetof(resona_platform, rgb_get) + sizeof(P->rgb_get)) return;
 const int r = static_cast<int>(((rgb >> 16) & 255) * k), g = static_cast<int>(((rgb >> 8) & 255) * k), b = static_cast<int>((rgb & 255) * k);
 const uint32_t key = static_cast<uint32_t>((r >> 3) << 10 | (g >> 3) << 5 | (b >> 3));
 if (key == s->lampKey) return;
 s->lampKey = key; resona_rgb_config c = resona_rgb_solid(static_cast<uint8_t>(r), static_cast<uint8_t>(g), static_cast<uint8_t>(b), 100);
 const int rc = P->rgb_set(&c); if (rc < 0 && rc != RESONA_ERR_STATE) failed(s, rc);
}
uint32_t hsv(float hdeg) {   // full saturation and value
 float h = hdeg - 360.f * static_cast<float>(static_cast<int>(hdeg / 360.f)); const int i = static_cast<int>(h / 60.f); const float f = h / 60.f - i;
 const int q = static_cast<int>(255 * (1 - f)), t = static_cast<int>(255 * f);
 const int rgb[6][3] = {{255, t, 0}, {q, 255, 0}, {0, 255, t}, {0, q, 255}, {t, 0, 255}, {255, 0, q}};
 const int *c = rgb[i % 6]; return static_cast<uint32_t>(c[0] << 16 | c[1] << 8 | c[2]);
}

void draw(State *s) {
 uint16_t *px = nullptr; uint32_t stride = 0; resona_ui *ui = P->screen_begin(&px, &stride); if (!ui || !px) return;
 type::Target T = {px, stride, 0, 0, 320, 172};
 if (!s->h) { ui->clear(ui->ctx, BG); ui->text(ui->ctx, 6, 80, 1, WHITE, "NOT ENOUGH APP MEMORY"); P->screen_present(0, 0, 0, 0); return; }
 Heap &h = *s->h; const uint16_t a = acc(s); const uint32_t accRgb = kGenres[s->genre].acc;
 for (int y = 0; y < 172; ++y) { uint16_t *row = px + y * stride; const uint16_t *src = h.grid + y * 320; for (int x = 0; x < 320; ++x) row[x] = src[x]; }
 // DROP: one beat of accent with the slam letters
 if (s->dropShow) {
  if (s->step - s->dropStep < 4) {
   type::rect(T, 0, 0, 320, 172, a);
   const char *d = "DROP"; for (int i = 0; i < 4; ++i) type::stamp(T, type::get(h.num, h.pool, d[i]), 12 + 76 * i, 11, INK);
   lamp(s, 1, hsv(static_cast<float>(s->ms) * .36f));
   demo_capture::remember(px, stride); P->screen_present(0, 0, 0, 0); return;
  }
  s->dropShow = 0; s->slam = 0;
 }
 if (s->boardFrame >= 0 && s->pendingGenre >= 0) {
  banner(s, T, kGenres[s->genre].name); board(s, T); problem(s, T);
  lamp(s, .12f + .88f * s->kickFlash, accRgb);
  demo_capture::remember(px, stride); P->screen_present(0, 0, 0, 0); return;
 }
 if (s->building) {
  const float bt = (s->step - s->buildStep) / 64.f < 0 ? 0 : ((s->step - s->buildStep) / 64.f > 1 ? 1 : (s->step - s->buildStep) / 64.f);
  banner(s, T, "BUILD");
  const int want = 6 + type::rnd(12 * bt);
  while (s->nLines < want && s->nLines < 18) { Line &l = s->lines[s->nLines++]; l.x = 320 + rnd01(s) * 200; l.y = static_cast<int16_t>(4 + rnd01(s) * 150); l.len = static_cast<int16_t>(24 + rnd01(s) * 72); l.th = static_cast<int16_t>(1 + rnd01(s) * 3); l.v = 16 + rnd01(s) * 16; l.white = rnd01(s) < .35f; }
  type::clip(T, 26, 0, 166, 162);
  for (int i = 0; i < s->nLines; ++i) { Line &l = s->lines[i]; l.x -= l.v; if (l.x + l.len < 26) { l.x = 320 + rnd01(s) * 60; l.y = static_cast<int16_t>(4 + rnd01(s) * 150); } type::rect(T, type::rnd(l.x), l.y, l.len, l.th, l.white ? WHITE : a); }
  type::noclip(T);
  const int left = 64 - (s->step - s->buildStep) > 0 ? 64 - (s->step - s->buildStep) : 0;
  uint32_t r = static_cast<uint32_t>(s->step * 7 + 3) * 2654435761u;              // a 2 px shake per 16th
  const int dy = static_cast<int>((r >> 8) % 5) - 2, dx = static_cast<int>((r >> 16) % 5) - 2;
  number(s, T, left, a, false, dy, dx);
  roster(s, T, true, bt);
  type::rect(T, 26, 164, 294, 8, BG);
  const int fx = 26 + type::rnd(294 * bt); type::rect(T, 26, 164, fx - 26, 8, a); type::rect(T, fx < 318 ? fx : 318, 164, 2, 8, WHITE);
  problem(s, T);
  const float rate = 1 + 2 * bt < 3 ? 1 + 2 * bt : 3; lamp(s, (static_cast<int>(s->ms / 1000.f * rate * 2) % 2) ? 1.f : .1f, accRgb);
  demo_capture::remember(px, stride); P->screen_present(0, 0, 0, 0); return;
 }
 s->nLines = 0;
 const bool stopped = s->stopped != 0;
 const bool fillNow = s->fillBar >= 0 && s->fillBar == (s->step < 0 ? 0 : s->step >> 4);
 banner(s, T, stopped ? "STOP" : s->scratch ? "SCRATCH" : fillNow ? "FILL" : kGenres[s->genre].name);
 // the numerals: slam after a drop, jolt on kicks and guitar hits, sink on a tape stop
 int dy = 0; uint16_t col = stopped ? DARK : WHITE;
 if (s->slam >= 0) { static const int k[3] = {-12, 4, 0}; dy = k[s->slam]; s->slam = s->slam < 2 ? s->slam + 1 : -1; }
 else if ((s->kickFlash > .7f && !s->kill) || s->gtrJolt > 0) dy = 3;
 if (s->gtrJolt > 0) --s->gtrJolt;
 if (s->brake >= 0) { dy = s->brake * 160 / 700; col = CONC; }
 type::clip(T, 26, 0, 170, 158); number(s, T, s->energy, col, true, dy, type::rnd(s->scratchX)); type::noclip(T);
 ticks(s, T, s->energy);
 if (s->topDown && (s->filterTouched || s->filter != 0)) filterStrip(s, T);
 else if (s->topDown && !s->bigDown && !stopped && s->ms - s->topDownAt > 200) {   // holding top: what the big knob does now
  restoreGrid(s, T, 196, 144, 124, 28);
  type::text(T, h.m8, h.pool, "BIG TURN = FILTER", 196, 148, WHITE); type::text(T, h.m8, h.pool, "BIG CLICK = KILL", 196, 162, WHITE);
 } else { beats(s, T); hint(s, T); }
 if (s->filter != 0 && !s->topDown) filterTag(s, T, 196, 132, 8);
 roster(s, T, false, 0);
 if (stopped) { type::rect(T, 196, 160, 124, 12, BG); type::text(T, h.m8, h.pool, "ANY KNOB = PLAY", 196, 162, WHITE); }
 problem(s, T);
 // the lamp: an accent flash on every kick; one bar of rainbow after a drop; a slow breathe in attract
 if (s->rainbowUntil > s->step) lamp(s, 1, hsv(static_cast<float>(s->ms) * .36f));
 else if (stopped) lamp(s, 0, accRgb);
 else if (s->brake >= 0) lamp(s, 1 - s->brake / 700.f, accRgb);
 else if (s->attract) { const float bar = stepMs(s) * 4, ph = s->t - bar * static_cast<float>(static_cast<int>(s->t / bar)); lamp(s, .25f + .25f * resona_cosf(ph / bar * 6.2831853f), accRgb); }
 else lamp(s, .12f + .88f * s->kickFlash, accRgb);
 demo_capture::remember(px, stride); P->screen_present(0, 0, 0, 0);
}

int init(void *p, const resona_platform *platform) {
 P = platform; State *s = static_cast<State *>(p);
 s->energy = 2; s->pendingGenre = -1; s->step = -1; s->bar = 1; s->slam = -1; s->rainbowUntil = -1; s->boardFrame = -1;
 s->brake = -1; s->bothAt = -1; s->attract = 1; s->seed = 0x48595045u;
 P->input_config(RESONA_INPUT_SELECTOR, RESONA_INPUT_DOUBLE); P->input_config(RESONA_INPUT_CUSTOMIZE, 0);
 // the boot splash (tools/make_splash.py): written only when it differs; it plays from the next power-up
 { const int rc = resona_startup_splash_apply(P, kSplash, sizeof kSplash);
   if (rc == RESONA_STARTUP_REBOOT_REQUIRED) P->log("Hype Dial: boot splash updated; it plays from the next power-up");
   else if (rc < 0 && rc != RESONA_ERR_UNSUPPORTED) P->log("Hype Dial: boot splash not applied"); }
 s->rollIdx = -1; s->fillBar = -1;
 { Saved v; s->sv = {kMagic, 65};
   if (P->app_store_read(0, &v, sizeof v) == RESONA_OK && v.magic == kMagic) s->sv.level = clampi(v.level, 0, 100); }
 { BackingSaved v; s->backing = 100;
   if (P->app_store_read(sizeof(Saved), &v, sizeof v) == RESONA_OK && v.magic == kBackingMagic) s->backing = clampi(v.backing, 0, 150); }
 bind(s); mirror(s);
 s->h = static_cast<Heap *>(P->alloc(sizeof(Heap), 4));
 if (s->h) {
  Heap &h = *s->h;
  for (int y = 0; y < 172; ++y) for (int x = 0; x < 320; ++x) h.grid[y * 320 + x] = (x % 8 == 0 || y % 8 == 0) ? GRID : BG;
  h.pool = {h.data, 0, kPoolBytes};
  type::face(h.num, 150, 72, 0); type::face(h.w40, 40, 20, 4); type::face(h.w20, 20, 11, 3); type::face(h.w16, 16, 9, 3);
  type::face(h.w12, 12, 7, 2); type::face(h.m10, 10, 6, 2); type::face(h.m8, 8, 5, 2);
  for (const char *c = "0123456789DROP"; *c; ++c) type::get(h.num, h.pool, *c);   // the big ones up front
 } else P->log("Hype Dial: app heap allocation failed");
 return 0;
}
#ifdef RESONA_DEMO_BENCH
// Bench stats on the capture channel (as in Night Rider and CREATURE): F0 7D 52 42 58 53 03 token F7 reads,
// 04 resets; the reply is 11 values as 4 x 7-bit groups each.
bool stats(State *s, const resona_app_event *e) {
 if (e->kind != RESONA_APP_EV_MIDI || e->u.midi.len < 9) return false;
 const uint8_t *m = e->u.midi.data;
 if (m[0] != 0xf0 || m[1] != 0x7d || m[2] != 0x52 || m[3] != 0x42 || m[4] != 0x58 || m[5] != 0x53 || (m[6] != 3 && m[6] != 4)) return false;
 if (m[6] == 4) { s->stDraws = s->stDrawSum = s->stDrawMax = s->stSlow = s->stFrames = s->stFrameMax = 0; s->stResetAt = P->millis(); }
 const uint32_t v[11] = {s->stDraws, s->stDrawSum, s->stDrawMax, s->stSlow, s->stFrames, s->stFrameMax, 33, s->ms, 0, 0, P->millis() - s->stResetAt};
 uint8_t out[60] = {0xf0, 0x7d, 0x52, 0x42, 0x58, 0x53, 0x40, m[6], m[7]}; int k = 9;
 for (int i = 0; i < 11; ++i) for (int b = 0; b < 4; ++b) out[k++] = static_cast<uint8_t>((v[i] >> (7 * b)) & 127);
 out[k++] = 0xf7; (void)P->midi_send(RESONA_MIDI_USB, out, static_cast<uint32_t>(k)); return true;
}
#endif
void event(void *p, const resona_app_event *ev) {
 State *s = static_cast<State *>(p);
#ifdef RESONA_DEMO_BENCH
 if (stats(s, ev)) return;
#endif
 if (demo_capture::event(P, ev)) return;
 ctl::Msg m;
 if (ctl::receive(P, s->link, kBind, kBinds, ev, m)) { if (m.changed) apply(s, m); return; }
 if (ev->kind == RESONA_APP_EV_INPUT) {
  const int enc = static_cast<int>(ev->u.input.input); if (enc < 0 || enc > 1) return;
  if (ev->u.input.kind == RESONA_EVENT_TURN) turn(s, enc, ev->u.input.value);
  else if (ev->u.input.kind == RESONA_EVENT_CLICK) click(s, enc);
  else if (ev->u.input.kind == RESONA_EVENT_LONG) longPress(s, enc);
  else if (ev->u.input.kind == RESONA_EVENT_DOUBLE) doubleClick(s, enc);
 } else if (ev->kind == RESONA_APP_EV_KEY) { if (ev->u.key.key < 2) key(s, static_cast<int>(ev->u.key.key), ev->u.key.down != 0); }
#ifdef RESONA_DEMO_BENCH
 else if (ev->kind == RESONA_APP_EV_MIDI) {      // bench only: channel 16 CC20..29 rehearse the encoders and keys
  const uint8_t *m = ev->u.midi.data;
  if (ev->u.midi.len >= 3 && m[0] == 0xbf) {
   const int cc = m[1], v = m[2];
   if (cc == 20 || cc == 21) turn(s, cc - 20, v - 64);
   else if (cc == 22 || cc == 23) key(s, cc - 22, v >= 64);
   else if ((cc == 24 || cc == 25) && v) click(s, cc - 24);
   else if ((cc == 26 || cc == 27) && v) longPress(s, cc - 26);
   else if ((cc == 28 || cc == 29) && v) doubleClick(s, cc - 28);
  }
 }
#endif
 else if (ev->kind == RESONA_APP_EV_CHANGED) {
  // our own parameter writes also raise BLOCK changes; only a preset change needs a fresh identity check
  if (ev->u.changed.what == RESONA_CHANGED_PRESET) { s->ready = 0; s->missing = 1; s->nextTry = 0; }
 } else if (ev->kind == RESONA_APP_EV_AUDIO) {   // the OS bypassed the engine for its CPU budget: back off longer on each trip
  if (ev->u.audio.tripped_role >= 0 || (s->ready && trip_guard::faulted(P, g_audio.h[0]))) trip(s);
 }
}
void frame(void *p, uint32_t ms) {
 if (demo_capture::paused(P)) return;
 State *s = static_cast<State *>(p); s->ms += ms; bootstrap(s);
 // the clock always runs, like the engine's sequencer (a stop only silences it)
 s->t += static_cast<float>(ms);
 while (static_cast<int>(s->t / stepMs(s)) > s->step) onStep(s, ++s->step);
 if (s->brake >= 0) { s->brake += static_cast<int32_t>(ms); if (s->brake > 700) { s->brake = -1; s->stopped = 1; } }
 if (s->in >= 2) { s->idle = 0; s->attract = 0; }
 else if (s->idle <= 40000) s->idle += ms;
 if (s->idle > 40000) s->attract = 1; // quiet screen prompt; keep the chosen genre and ENERGY
 if (s->bothAt >= 0 && s->ms - static_cast<uint32_t>(s->bothAt) >= 3000) { s->bothAt = -2; reset(s); }   // staff reset
 if (s->bigDown && !s->bigTurned && !s->building && s->ms - s->bigDownAt >= 300 && !s->topDown && !s->stopped && s->brake < 0 && !s->dropShow) {
  s->building = 1; s->buildRelease = 0; s->buildStep = s->step + 1; s->nLines = 0; s->suppressBig = 1; wake(s); param(s, BUILD, 1);
 }
 // after four bars the build lets go by itself: release a bar early so the engine drops on the same downbeat
 if (s->building && !s->buildRelease && s->step - s->buildStep >= 48) { s->buildRelease = 1; param(s, BUILD, 0); }
 if (s->boardFrame >= 0) ++s->boardFrame;
 for (int i = 0; i < 7; ++i) if (s->flash[i]) --s->flash[i];
 P->meters(&s->in, &s->out, &s->cpu);
 s->gtr = s->in / 9.f > s->gtr * .96f ? s->in / 9.f : s->gtr * .96f;
 if (s->in >= 5) s->gtrJolt = 2;
 if (s->in >= 6 && s->lastIn < 6 && s->energy >= 8 && !s->building && !s->stopped && s->brake < 0) s->stabUntil = s->ms + 450;   // the engine fires the stab
 s->lastIn = s->in;
 if (s->saveAt && static_cast<int32_t>(s->ms - s->saveAt) >= 0) {
  s->saveAt = 0;
  (void)P->app_store_write(0, &s->sv, sizeof s->sv);
  const BackingSaved v = {kBackingMagic, s->backing};
  (void)P->app_store_write(sizeof(Saved), &v, sizeof v);
 }
 mirror(s); ctl::sync(P, s->link, kBind, kBinds, s->ms);
 s->scratchX *= resona_expf((s->scratch ? -1.2f : -3.912f) * static_cast<float>(ms) / 140.f);
 s->kickFlash *= resona_expf(-3.912f * static_cast<float>(ms) / 120.f);
 { const uint32_t t = P->millis(); if (s->lastFrameAt && t - s->lastFrameAt > s->stFrameMax) s->stFrameMax = t - s->lastFrameAt; s->lastFrameAt = t; ++s->stFrames; }
 const uint32_t t0 = P->millis(); draw(s); const uint32_t took = P->millis() - t0;
 ++s->stDraws; s->stDrawSum += took; if (took > s->stDrawMax) s->stDrawMax = took; if (took >= 20) ++s->stSlow;
}
const resona_app app = {RESONA_PLATFORM_VERSION, sizeof(resona_app), RESONA_APP_ID, RESONA_APP_NAME, RESONA_APP_VERSION,
                        sizeof(State), sizeof(Heap) + 4096, init, event, frame};
}
extern "C" const resona_app *resona_app_get() { return &app; }
