// Original MIT mono performance engine. All voices and guitar share ONE output.
#include "dsp.h"
#include <resona/math.h>
namespace {
// The per-sample helpers are forced inline: at -Os the device compiler otherwise calls them on every sample, which
// costs far more on the box's in-order core than the arithmetic itself.
#define HOT inline __attribute__((always_inline))
HOT float clamp(float v,float lo,float hi){return v<lo?lo:(v>hi?hi:v);}
HOT float absf(float x){return x<0?-x:x;}
HOT float flush(float x){return absf(x)<1.e-20f?0:x;}
HOT float clip(float x){return x/(1+absf(x));}
// Smooth parabolic oscillator, deliberately inexpensive and identical on host/device.
HOT float sine(float phase){float x=phase*2-1;float y=4*x*(1-absf(x));return y*(.775f+.225f*absf(y));}
HOT float phase(float &p,float inc){p+=inc;if(p>=1)p-=1;return p;}
float midi(int n){return 440.f*resona_exp2f((n-69)/12.f)/48000.f;}
void tune(Dsp &d){
 static const int roots[8]={45,48,41,43,50,46,43,40};
 // Four bars per style. The chord, bass movement, melody shape, and rhythmic mask all change independently.
 static const int chord[6][4]={{0,-4,3,-2},{0,0,-2,0},{0,-4,-2,3},{0,-4,3,-2},{0,5,0,-2},{0,-2,-4,-2}};
 static const int bass[6][8]={{0,0,7,0,0,7,3,7},{0,0,0,7,0,0,10,7},{0,0,3,0,7,3,0,10},
                              {0,7,0,3,7,0,10,7},{0,0,3,7,0,10,7,3},{0,7,0,5,7,3,0,7}};
 static const int lead[6][8]={{0,3,7,10,7,3,12,10},{0,0,7,10,12,10,7,3},{7,3,0,3,10,7,3,0},
                              {0,7,10,12,7,3,10,15},{0,3,5,7,10,7,5,3},{0,5,7,10,12,10,7,5}};
 const int phrase=(d.bar?d.bar-1:0)&3, g=d.genre;
 const int root=roots[(int)d.v[P_TUNE]&7]+chord[g][phrase];
 const int shift=phrase==1?2:phrase==3?4:0;
 for(int i=0;i<8;++i){
  d.bassNotes[i]=midi(root+bass[g][(i+shift)&7]);
  const int ending=phrase==2&&i>=6?5:phrase==3&&i==7?12:0;
  d.arpNotes[i]=midi(root+24+lead[g][(i+shift)&7]+ending);
 }
 d.padIncrement=midi(root+12);d.padIncrement2=midi(root+(g==4?16:19));
}
// DJ filter coefficients from the smoothed amount: low-pass 18 kHz -> 200 Hz to the left, high-pass
// 20 Hz -> 5 kHz to the right, a little resonance either way (trapezoidal SVF).
void djfilter(Dsp &d){
 const float f=d.filter, a=absf(f);
 const float hz=f<0?18000.f*resona_exp2f(a*resona_log2f(200.f/18000.f)):20.f*resona_exp2f(a*resona_log2f(5000.f/20.f));
 const float x=3.141592654f*clamp(hz,20,20000)/48000.f, g=resona_sinf(x)/resona_cosf(x);
 d.fk=1/(1.1f+.6f*a);d.fa1=1/(1+g*(g+d.fk));d.fa2=g*d.fa1;d.fa3=g*d.fa2;
}
void retune(Dsp &d){d.bassIncrement=d.bassNotes[(d.step/2)&7];d.arpIncrement=d.arpNotes[d.step&7];}
void triggerKick(Dsp &d,float velocity){
 d.kick=velocity;
 d.kickPhase=d.kickLow=0;
 d.kickAttack=0;
}
void beat(Dsp &d){
 if(d.step==0){
  d.genre=d.pendingGenre;
  static const uint32_t steps[6]={5806,5538,8000,4186,6545,6667};d.stepLength=steps[d.genre];
  if(d.pendingDrop){d.energy=10;d.v[P_ENERGY]=10;d.pendingDrop=0;d.snare=.35f;}
  d.filling=d.fillPending;d.fillPending=0;   // a fill takes the whole next bar
  ++d.bar;
  tune(d);
 }
 if(d.stabPending){d.stab=.75f;d.stabPending=0;}   // the guitar's hard pick lands on this 16th
 // Sequence layers use the selected detent; audio gains still use the smoothed value.
 const int e=(int)(d.v[P_ENERGY]+.5f);
 bool kick=d.genre==2?(d.step==0||d.step==7||d.step==10):
           d.genre==3?(d.step==0||d.step==6||d.step==10):
           d.genre==4?(d.step==0||d.step==3||d.step==8||d.step==11):
           d.genre==5?(d.step==0||d.step==6||d.step==10||d.step==14):(d.step%4==0);
 bool back=d.step==4||d.step==12;
 if(d.filling&&d.build<.05f){
  // one bar of fill: four toms falling, then a snare crescendo
  static const float toms[4]={220.f,180.f,148.f,118.f};
  if(d.step==0&&!d.kill)triggerKick(d,1.f);
  if(d.step<8&&d.step%2==0){d.tom=.9f;d.tomPhase=0;d.tomInc=toms[d.step/2]/48000.f;}
  if(d.step>=8)d.snare=.3f+.05f*(float)(d.step-8);
  if(d.step%2==0)d.hat=.3f;
 }else{
  if(e==0&&d.step==0&&!d.kill)triggerKick(d,.48f); // the quietest setting still has a pulse
  else if(kick&&e>=1&&!d.kill)triggerKick(d,e==1?.65f:d.genre==1?1.2f:1.f);
  // A genre must be recognizable at the startup ENERGY=2, before bass and arp arrive.
  if(back){
   if(d.genre>=2&&d.genre!=5){if(e>=2)d.snare=e>=6?.7f:(d.genre==4?.42f:.52f);}
   else if(e>=5){d.snare=.7f;}
   if(e>=6||(d.genre==0&&e>=5)||(d.genre==4&&e>=2))d.clap=d.genre==4?.55f:.7f;
  }
  if(d.genre==4&&e>=2&&d.step==15)d.snare=.25f; // Funk ghost note
  if(d.genre==5&&e>=2){
   if(d.step==3||d.step==7||d.step==11||d.step==15){d.snare=e>=6?.7f:.52f;d.snarePhase=0;}
   if(d.step==0||d.step==3||d.step==6||d.step==10||d.step==12)d.clap=.48f; // 3-2 clave
  }
  if(e==1&&d.step==8)d.hat=.18f;
  if(e>=2){
   if(d.genre==0){if(d.step%4==2)d.hat=.48f;}
   else if(d.genre==1){d.hat=d.step%2?.20f:.42f;}
   else if(d.genre==2){if(d.step%2==0||d.step==7||d.step==15)d.hat=d.step%4==2?.40f:.25f;}
   else if(d.genre==3){d.hat=d.step%2?.22f:.44f;}
   else if(d.genre==4){if(d.step%4==2||d.step==7||d.step==15)d.hat=d.step%4==2?.40f:.22f;}
   else if(d.step%2==0)d.hat=d.step%4==2?.42f:.25f; // Latin shaker
  }
 }
 const bool funkBass=d.step==0||d.step==3||d.step==6||d.step==10||d.step==13;
 if(d.genre==1){if(e>=2&&(d.step==0||d.step==8||(e>=4&&d.step%4==0)))d.bass=e>=4?.7f:.45f;}
 else if(d.genre==2){if(e>=2&&(d.step==0||d.step==7||d.step==10||(e>=4&&d.step==14)))d.bass=e>=4?.7f:.50f;}
 else if(d.genre==4){if(e>=2&&funkBass)d.bass=e>=4?.7f:.50f;}
 else if(d.genre==5){if(e>=2&&(d.step==0||d.step==6||d.step==10||d.step==14))d.bass=e>=4?.7f:.48f;}
 else if(e>=4&&d.step%2==0)d.bass=.7f;
 static const uint16_t melody[6][4]={{0x4411,0x1491,0x5114,0x9144},{0x5555,0x5155,0x5551,0x5d55},
                                     {0x2081,0x2811,0x8091,0x2141},{0xa249,0x9249,0x4a49,0xa291},
                                     {0x4491,0x24a1,0x4911,0x64a1},{0x9525,0x924d,0x5529,0x9265}};
 const int phrase=(d.bar-1)&3;
 if((e>=2&&(melody[d.genre][phrase]&(1u<<d.step)))||(e==1&&d.step==0))d.arp=e>=8?.46f:e>=2?.31f:.20f;
 if(e>=9&&d.step%4==2)d.stab=.6f;
 if(d.build>.05f){
  // the build's snare roll, at the rate the top knob set (or every 16th)
  static const uint32_t every[5]={1,4,2,1,1};
  if(d.step%every[d.roll]==0)d.snare=.28f+d.build*.5f;
  d.hat=.35f;
 }
 retune(d);d.step=(d.step+1)&15;
}
}
void dsp_init(Dsp &d){for(uint32_t i=0;i<P_COUNT;++i)d.v[i]=kParams[i].def;dsp_reset(d);}
void dsp_reset(Dsp &d){
 d.energy=d.v[P_ENERGY];d.level=d.v[P_LEVEL]*.01f;d.backing=d.v[P_BACKING]*.01f;d.stopGain=d.v[P_STOP]>.5f?0:1;
 d.kickAttack=48;
 d.kick=d.snare=d.hat=d.clap=d.bass=d.arp=d.pad=d.stab=0;
 d.kickPhase=d.snarePhase=d.metalPhase=d.bassPhase=d.arpPhase=d.padPhase=d.padPhase2=0;
 d.guitarTone=d.guitarDC=d.noiseLP=d.drumDark=d.drumBright=d.kickLow=d.bassLP=0;d.gate=1;d.build=0;d.lastBuild=d.v[P_BUILD];
 d.scratchHead=d.scratchVel=d.tom=d.tomPhase=d.tomInc=d.pickEnv=d.scratchMix=0;d.scratchAnchor=d.scratchBeatLength=0;d.scratching=0;
 d.roll=(int)(d.v[P_ROLL]+.5f);d.rollLive=0;d.kill=d.v[P_KILL]>.5f;d.fillToken=d.v[P_FILL];d.filling=d.fillPending=0;d.pickArmed=1;d.stabPending=0;
 d.stutterToken=d.v[P_STUTTER];d.pitchBrake=1;d.filter=d.v[P_FILTER]*.01f;d.fs1=d.fs2=0;djfilter(d);d.seed=0x7265736fu;d.clock=0;d.stepLength=5806;
 d.step=d.bar=d.write=d.historyFilled=d.replay=d.replayBase=d.replayLength=d.replayLeft=0;
 d.genre=d.pendingGenre=(int)d.v[P_GENRE];d.pendingDrop=0;d.stopped=d.v[P_STOP]>.5f;
 for(uint32_t i=0;i<32768;++i)d.history[i]=0;
 tune(d);retune(d);
}
void dsp_param(Dsp &d,uint32_t index,float value){
 if(index>=P_COUNT)return;
 value=clamp(value,kParams[index].min,kParams[index].max);d.v[index]=value;
 if(index==P_GENRE)d.pendingGenre=(int)(value+.5f);
 if(index==P_BUILD){if(value<.5f&&d.lastBuild>=.5f)d.pendingDrop=1;d.lastBuild=value;}
 if(index==P_STUTTER&&value!=d.stutterToken){
  d.stutterToken=value;
  const uint32_t length=d.stepLength/2;
  if(!d.rollLive&&d.historyFilled>=length){
   d.replayLength=length;
   d.replayBase=(d.write+32768-length)&32767;
   d.replay=0;
   d.replayLeft=d.stepLength*2;
  }
 }
 if(index==P_STOP)d.stopped=value>.5f;
 if(index==P_TUNE)tune(d);   // the playing notes keep their pitch; the next step picks up the new root (beat -> retune)
 if(index==P_SCRATCH&&value<.5f)d.scratching=0;
 if(index==P_ROLL)d.roll=(int)(value+.5f);
 if(index==P_KILL)d.kill=value>.5f;
 if(index==P_FILL&&value!=d.fillToken){d.fillToken=value;d.fillPending=1;}
}
void dsp_process(Dsp &d,const float *in,float *out,uint32_t frames){
 // Only start a repeat once the complete requested slice has been recorded.
 const int want=d.build>.05f&&d.v[P_BUILD]>.5f?d.roll:0;
 if(want!=d.rollLive){
  if(!want){
   d.rollLive=0;
   d.replayLeft=0;
  }else{
   const uint32_t length=d.stepLength*4u>>(want-1);
   if(d.historyFilled>=length){
    d.rollLive=want;
    d.replayLength=length;
    d.replayBase=(d.write+32768-length)&32767;
    d.replay=0;
    d.replayLeft=0x7fffffff;
   }
  }
 }
 const uint32_t beatLength=d.stepLength*4u;
 if(d.v[P_SCRATCH]>.5f&&!d.scratching&&!d.replayLeft&&d.historyFilled>=beatLength){
  d.scratching=1;
  d.scratchAnchor=d.write;
  d.scratchBeatLength=beatLength;
  d.scratchHead=(float)beatLength*.5f;
  d.scratchVel=0;
 }
 for(uint32_t i=0;i<frames;++i){
  d.energy+=(d.v[P_ENERGY]-d.energy)*.0008f;
  d.level+=(d.v[P_LEVEL]*.01f-d.level)*.001f;
  d.backing+=(d.v[P_BACKING]*.01f-d.backing)*.001f;
  d.stopGain+=((d.stopped?0.f:1.f)-d.stopGain)*.00016f;
  d.pitchBrake+=((d.stopped?.08f:1.f)-d.pitchBrake)*.00012f;
  d.build+=((d.v[P_BUILD]>.5f?1.f:0.f)-d.build)*.0001f;
  if(d.v[P_FILTER]!=0||d.filter!=0){             // the DJ filter costs nothing while it is centred
   d.filter+=(d.v[P_FILTER]*.01f-d.filter)*.002f;
   if(d.v[P_FILTER]==0&&absf(d.filter)<.0005f){d.filter=0;d.fs1=d.fs2=0;}
   if((d.clock&31)==0)djfilter(d);
  }
  if(d.clock==0)beat(d);
  if(d.roll==4&&d.build>.05f&&d.clock==d.stepLength/2)d.snare=.28f+d.build*.5f;   // 1/32: a second hit per 16th
  if(++d.clock>=d.stepLength)d.clock=0;
  d.seed^=d.seed<<13;d.seed^=d.seed>>17;d.seed^=d.seed<<5;
  float noise=(float)(int32_t)d.seed*(1.f/2147483648.f);
  d.noiseLP+=.13f*(noise-d.noiseLP);
  d.drumDark+=.035f*(noise-d.drumDark);
  d.drumBright+=.38f*(noise-d.drumBright);
  const float high=noise-d.noiseLP,mid=d.noiseLP-d.drumDark,bright=noise-d.drumBright;
  static const float kKickBase[6]={44.f,52.f,38.f,48.f,46.f,50.f};
  const float kp=phase(d.kickPhase,(kKickBase[d.genre]+130.f*d.kick*d.kick)*d.pitchBrake/48000.f);
  const float kb=sine(kp);
  d.kickLow+=.042f*(kb-d.kickLow);
  const float p2=kp*2.f, harmonic=sine(p2>=1.f?p2-1.f:p2);
  float kick=0;
  switch(d.genre){
   case 0:kick=(d.kickLow*.37f+kb*.16f+harmonic*.055f+high*d.kick*.018f)*d.kick;break; // rounded, layered house
   case 1:kick=(clip(kb*2.4f)*.57f+harmonic*.12f+bright*d.kick*.042f)*d.kick;break; // clipped machine attack
   case 2:kick=(d.kickLow*.46f+kb*.12f+mid*d.kick*.012f)*d.kick;break; // deep sub-like hip-hop
   case 3:kick=(kb*.40f+harmonic*.11f+bright*d.kick*.066f)*d.kick;break; // punchy DnB
   case 4:kick=(kb*.32f+harmonic*.14f+mid*d.kick*.028f)*d.kick;break; // woody Funk
   default:kick=(d.kickLow*.24f+kb*.24f+harmonic*.08f+mid*d.kick*.040f)*d.kick;break; // Latin hand drum
  }
  // Keep the pitch sweep, but leave headroom and fade in the attack over 1 ms.
  kick*=.82f;
  if(d.kickAttack<48){
   const float attack=(float)d.kickAttack++*(1.f/48.f);
   kick*=attack*attack*(3.f-2.f*attack);
  }
  static const float kSnareHz[6]={185.f,630.f,170.f,235.f,340.f,275.f};
  const float body=sine(phase(d.snarePhase,kSnareHz[d.genre]/48000.f));
  const float metal=sine(phase(d.metalPhase,(d.genre==1?3950.f:d.genre==3?5100.f:2700.f)/48000.f));
  const float snap=d.snare*d.snare;
  float snare=0,hat=0,clap=0;
  switch(d.genre){
   case 0:snare=body*d.snare*.035f+high*(snap*.12f+d.snare*.045f);hat=high*d.hat*.075f;clap=(high*.65f+mid*.35f)*d.clap*.10f;break;
   case 1:snare=metal*snap*.075f+bright*(snap*.14f+d.snare*.08f);hat=(bright*.55f+metal*.25f)*d.hat*.11f;clap=(high+metal*.2f)*d.clap*.09f;break;
   case 2:snare=body*d.snare*.085f+mid*(snap*.11f+d.snare*.07f);hat=(mid*.7f+high*.15f)*d.hat*.085f;clap=mid*d.clap*.12f;break;
   case 3:snare=body*d.snare*.055f+bright*(snap*.19f+d.snare*.08f);hat=bright*d.hat*.13f;clap=bright*d.clap*.10f;break;
   case 4:snare=body*d.snare*.11f+high*(snap*.12f+d.snare*.035f);hat=(mid*.45f+high*.25f+metal*.30f)*d.hat*.10f;clap=(high*.65f+body*.35f)*d.clap*.10f;break;
   default:snare=body*d.snare*.10f+mid*(snap*.10f+d.snare*.04f);hat=(mid*.55f+metal*.45f)*d.hat*.10f;clap=(metal*.65f+bright*.35f)*d.clap*.09f;break;
  }
  static const float kKickDecay[6]={.99955f,.99948f,.99962f,.99935f,.99942f,.99945f};
  d.kick=flush(d.kick*kKickDecay[d.genre]);d.snare=flush(d.snare*(d.genre==5?.99955f:.99972f));d.hat=flush(d.hat*(d.genre==3?.9988f:.99918f));d.clap=flush(d.clap*(d.genre==5?.9988f:.99952f));
  d.bass=flush(d.bass*.99983f);d.arp=flush(d.arp*(d.genre==0?.99978f:d.genre==2?.99972f:.99962f));d.stab=flush(d.stab*.99978f);
  float bphase=phase(d.bassPhase,d.bassIncrement*d.pitchBrake);
  float bs=bphase*2-1;d.bassLP+=.075f*(bs-d.bassLP);
  float bass=(d.genre==2||d.genre==4||d.genre==5?sine(bphase):d.bassLP)*d.bass*(d.genre==2?.27f:.2f);
  float ap=phase(d.arpPhase,d.arpIncrement*d.pitchBrake);
  static const float kLeadGain[6]={.09f,.11f,.12f,.11f,.12f,.12f};
  const float leadWave=d.genre==0?sine(ap):d.genre==1?(ap<.28f?.55f:-.22f):d.genre==2?(sine(ap)*.7f+(ap<.5f?.16f:-.16f)):
                       d.genre==3?(ap<.22f?.65f:-.18f):d.genre==4?(ap<.18f?.8f:-.15f):sine(ap)*.8f;
  float arp=leadWave*d.arp*kLeadGain[d.genre];
  static const float kPad[6]={.025f,.009f,.007f,.006f,.008f,.012f};
  float pad=(sine(phase(d.padPhase,d.padIncrement*d.pitchBrake))+sine(phase(d.padPhase2,d.padIncrement2*d.pitchBrake)))*kPad[d.genre];
  if(d.energy<1.5f)pad*=.45f+.55f*d.energy/1.5f;
  static const float kDrive[6]={.8f,1.25f,.45f,.95f,.35f,.50f};
  float drive=1+d.energy*kDrive[d.genre];
  float x=in[i];d.guitarDC+=.002f*(x-d.guitarDC);x-=d.guitarDC;
  float wet=clip(x*drive)*(1+drive*.25f);
  static const float kTone[6]={.30f,.13f,.07f,.40f,.42f,.42f};
  d.guitarTone+=kTone[d.genre]*(wet-d.guitarTone);
  float beatPhase=(float)d.clock/(float)d.stepLength;
  float target=d.stopped?1.f:d.energy>=7?(beatPhase<.52f?1.f:.16f):
               (d.genre==3&&d.energy>=2?(beatPhase<.5f?1.f:.58f):
                (d.energy>=5?((d.step&1)||beatPhase<.8f?1.f:.4f):1.f));
  d.gate+=(target-d.gate)*.025f;
  static const float kWet[6]={.065f,.12f,.10f,.10f,.06f,.15f};
  const float guitarWet=clamp(d.energy*kWet[d.genre],0,.75f);
  float guitar=(x*(1-guitarWet)+d.guitarTone*guitarWet)*d.gate;
  float stab=(sine(d.padPhase)+sine(d.padPhase2))*d.stab*.1f;
  float tom=0;if(d.tom>0){tom=sine(phase(d.tomPhase,d.tomInc*(1+d.tom*.6f)))*d.tom*.38f;d.tom=flush(d.tom*.99935f);}   // only during a fill
  // a hard pick at energy 8+ fires the chord stab on the next 16th
  {const float a=absf(in[i]);d.pickEnv=a>d.pickEnv?a:d.pickEnv*.9996f;
   if(d.pickEnv>.3f&&d.pickArmed&&d.energy>7.5f&&d.build<.05f&&!d.stopped){d.stabPending=1;d.pickArmed=0;}
   else if(d.pickEnv<.1f)d.pickArmed=1;}
  float song=kick+snare+hat+clap+bass+arp+pad+stab+tom;
  song=song*(1-d.build*.75f)+(noise-d.noiseLP)*d.build*.055f;
  float result=guitar*.65f+song*d.stopGain*d.backing;   // STOP brakes and silences the track only; the guitar keeps playing
  if(d.filter!=0){// the DJ filter sweeps everything; near the centre it blends back to clean
   const float v3=result-d.fs2,v1=d.fa1*d.fs1+d.fa2*v3,v2=d.fs2+d.fa2*d.fs1+d.fa3*v3;
   d.fs1=flush(2*v1-d.fs1);d.fs2=flush(2*v2-d.fs2);
   const float filtered=d.filter<0?v2:result-d.fk*v1-v2,amount=clamp(absf(d.filter)*12,0,1);
   result+=amount*(filtered-result);
  }
  if(!d.scratching){
   if(d.replayLeft){
    float repeat=(float)d.history[(d.replayBase+d.replay)&32767]/32768.f;
    uint32_t edge=d.replay<d.replayLength-d.replay?d.replay:d.replayLength-d.replay;
    float win=edge<240?(float)edge/240.f:1.f;
    result=result*(1-.8f*win)+repeat*.8f*win;
    d.replay=(d.replay+1)%d.replayLength;if(d.replayLeft!=0x7fffffff)--d.replayLeft;
   }else if(d.scratchMix==0){
    d.history[d.write]=(int16_t)(clamp(result,-.999f,.999f)*32767.f);
    d.write=(d.write+1)&32767;
    if(d.historyFilled<32768)++d.historyFilled;
   }
  }
  if(d.scratching||d.scratchMix>0){
   // the record under the hand: the head chases the knob's position in the last beat; its speed is the pitch.
   // A 4 ms linear crossfade takes the live mix in and out of the record, so grabbing and letting go do not click.
   const float beatLen=(float)d.scratchBeatLength;
   const float knob=clamp(d.v[P_SCRATCH],1.f,33.f);
   const float target=beatLen*.5f-(knob-17.f)*beatLen/32.f;
   const float w=.0016f;d.scratchVel+=w*w*(target-d.scratchHead)-2*w*d.scratchVel;d.scratchHead+=d.scratchVel;
   const float pos=(float)(d.scratchAnchor+32768u)-clamp(d.scratchHead,1,beatLen);const uint32_t i0=(uint32_t)pos;const float fr=pos-(float)i0;
   const float a=(float)d.history[i0&32767],b=(float)d.history[(i0+1)&32767],speed=absf(d.scratchVel);
   d.scratchMix=clamp(d.scratchMix+(d.scratching?1.f/192.f:-1.f/192.f),0,1);
   result+=d.scratchMix*((a+(b-a)*fr)/32768.f*clamp(speed*1.5f,0,1)-result);
  }
  out[i]=clip(result*1.35f)*d.level;
 }
}
