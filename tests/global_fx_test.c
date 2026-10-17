#include <assert.h>
#include <math.h>
#include <stddef.h>
#include "global_fx_private.h"
static FxParticle particles[64];
static unsigned count;
static bool capture(const FxParticle *p) {
    assert(p && isfinite(p->position.x) && isfinite(p->position.y));
    assert(isfinite(p->velocity_x) && isfinite(p->velocity_y));
    assert(p->dust_type>0 && p->scale>0);
    assert(count<64); particles[count++]=*p; return true;
}
int main(void) {
    fx_global_bind(capture); fx_global_reset();
    FxNpcHit normal={2,12,{100,200},false,false,1000};
    assert(fx_global_npc_hit(&normal)); assert(count==4);
    assert(!fx_global_npc_hit(&(FxNpcHit){2,12,{100,200},false,false,1050}));
    assert(count==4);
    FxNpcHit kill={3,80,{120,210},true,true,1100};
    assert(fx_global_npc_hit(&kill)); assert(count==16);
    unsigned gold=0,purple=0;
    for(unsigned i=4;i<count;i++){if(particles[i].dust_type==228)gold++;if(particles[i].dust_type==27)purple++;}
    assert(gold==9 && purple==3);
    FxAmbientEvent snow={{0,0},480,800,FX_BIOME_SNOW,2000};
    fx_global_ambient(&snow); assert(count==17 && particles[16].dust_type==197);
    snow.at_ms=2500; fx_global_ambient(&snow); assert(count==17);
    snow.at_ms=2900; fx_global_ambient(&snow); assert(count==18);
    FxAmbientEvent jungle={{0,0},480,800,FX_BIOME_JUNGLE,4000};
    fx_global_ambient(&jungle); assert(count==19 && particles[18].dust_type==298);
    fx_global_reset(); assert(fx_global_npc_hit(&normal));
    assert(count==23);
    fx_global_bind(NULL); assert(!fx_global_npc_hit(&kill));
    return 0;
}
