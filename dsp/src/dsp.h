#ifndef HYPE_DSP_H
#define HYPE_DSP_H
#include <stdint.h>
#include "params.h"
struct Dsp {
 float v[P_COUNT], energy, level, backing, stopGain;
 float kick, snare, hat, clap, bass, arp, pad, stab;
 float kickPhase, snarePhase, metalPhase, bassPhase, arpPhase, padPhase, padPhase2;
 float bassIncrement, arpIncrement, padIncrement, padIncrement2;
 float bassNotes[8], arpNotes[8];
 float guitarTone, guitarDC, noiseLP, drumDark, drumBright, kickLow, bassLP, gate;
 float build, lastBuild, stutterToken, pitchBrake;
 float filter, fs1, fs2, fa1, fa2, fa3, fk;   // DJ filter: smoothed amount, SVF state and coefficients
 float scratchHead, scratchVel, fillToken, tom, tomPhase, tomInc, pickEnv, scratchMix;   // scratch, fill tom, guitar stab; scratchMix crossfades the scratch in and out
 uint32_t scratchAnchor, scratchBeatLength, kickAttack; int scratching, roll, rollLive, kill, filling, fillPending, pickArmed, stabPending;
 uint32_t seed, clock, stepLength, step, bar, write, historyFilled, replay, replayBase, replayLength, replayLeft;
 int genre, pendingGenre, pendingDrop, stopped;
 int16_t history[32768];   // the last beat even at 90 BPM: stutter, roll and scratch read it
};
void dsp_init(Dsp &d);
void dsp_reset(Dsp &d);
void dsp_param(Dsp &d,uint32_t index,float value);
void dsp_process(Dsp &d,const float *in,float *out,uint32_t frames);
#endif
