#ifndef HYPE_PARAMS_H
#define HYPE_PARAMS_H
#include <resona/module.h>
enum Param : uint32_t { P_ENERGY, P_GENRE, P_BUILD, P_STUTTER, P_STOP, P_TUNE, P_LEVEL, P_FILTER, P_SCRATCH, P_ROLL, P_KILL, P_FILL, P_BACKING, P_COUNT };
const resona_param kParams[P_COUNT] = {
 {"ENERGY",0,10,2,""}, {"GENRE",0,5,0,""}, {"BUILD",0,1,0,""},
 {"STUTTER",0,127,0,""}, {"STOP",0,1,0,""}, {"TUNE",0,7,0,""}, {"LEVEL",0,100,65,"%"},
 {"FILTER",-100,100,0,"%"},  // DJ filter on the whole mix: left low-pass, right high-pass, 0 clean
 {"SCRATCH",0,33,0,""},      // 0 off; 1..33 = the record at -16..+16 detents (17 = where it was grabbed)
 {"ROLL",0,4,0,""},          // while building: 0 the build's own roll, 1..4 = beat repeat 1/4, 1/8, 1/16, 1/32
 {"KILL",0,1,0,""},          // kick kill
 {"FILL",0,127,0,""},        // a token: any change plays a one-bar drum fill on the next bar
 {"BACKING",0,150,100,"%"}  // accompaniment level relative to the guitar
};
#endif
