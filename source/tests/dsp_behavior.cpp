#include "../dsp/src/dsp.h"
#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <memory>
static void run(Dsp&d,float value,float *out,int frames){float in[256];for(int i=0;i<256;++i)in[i]=value;for(int i=0;i<frames;i+=64)dsp_process(d,in,out,64);}
int main(){
 auto a=std::make_unique<Dsp>(),b=std::make_unique<Dsp>();dsp_init(*a);dsp_init(*b);
 float in[256],oa[256],ob[256];double diff=0,energy=0;
 dsp_param(*a,P_ENERGY,0);dsp_param(*b,P_ENERGY,10);
 for(int block=0;block<1500;++block){for(int i=0;i<64;++i)in[i]=.18f*std::sin((block*64+i)*.0576f);dsp_process(*a,in,oa,64);dsp_process(*b,in,ob,64);for(int i=0;i<64;++i){if(!std::isfinite(oa[i])||!std::isfinite(ob[i]))return 2;diff+=(oa[i]-ob[i])*(oa[i]-ob[i]);energy+=ob[i]*ob[i];}}
 double rmsDiff=std::sqrt(diff/96000);if(rmsDiff<.02)return 3;
 dsp_param(*b,P_STOP,1);run(*b,0,ob,192000);double stop=0;for(int i=0;i<64;++i)stop+=ob[i]*ob[i];stop=std::sqrt(stop/64);if(stop>1.e-5)return 4;
 dsp_init(*a);dsp_param(*a,P_BUILD,1);run(*a,0,oa,48000);dsp_param(*a,P_BUILD,0);int latency=0;while(a->pendingDrop&&latency<100000){run(*a,0,oa,64);latency+=64;}if(a->pendingDrop||a->v[P_ENERGY]!=10)return 5;
 dsp_init(*a);dsp_init(*b);for(int i=0;i<256;++i)in[i]=oa[i]=.2f*std::sin(i*.14f);dsp_process(*a,in,ob,256);dsp_process(*b,oa,oa,256);for(int i=0;i<256;++i)if(oa[i]!=ob[i])return 6;
 std::printf("{\"energy_0_vs_10_rms_difference\":%.6f,\"high_energy_rms\":%.6f,\"stop_tail_rms\":%.10f,\"drop_wait_frames\":%d,\"in_place_equal\":true}\n",rmsDiff,std::sqrt(energy/96000),stop,latency);
}
