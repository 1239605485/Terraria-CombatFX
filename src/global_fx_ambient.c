#include "global_fx_private.h"
#include <stdint.h>

static uint32_t rng=0x6a09e667u;
static uint32_t next_random(void) {
    rng^=rng<<13; rng^=rng>>17; rng^=rng<<5; return rng;
}
static int dust_for_biome(FxBiome biome) {
    switch(biome) {
        case FX_BIOME_SNOW: return 197;
        case FX_BIOME_HALLOW: return 66;
        case FX_BIOME_JUNGLE: return 298;
        case FX_BIOME_CORRUPTION: return 295;
        case FX_BIOME_CRIMSON: return 296;
        case FX_BIOME_DESERT: return 36;
        case FX_BIOME_UNDERWORLD: return 36;
        default: return 304;
    }
}
void fx_global_ambient_emit(const FxAmbientEvent *event) {
    uint32_t a=next_random(), b=next_random();
    float x=(float)(a%10000u)/10000.0f*(float)event->width;
    float y=(float)(b%10000u)/10000.0f*(float)event->height;
    float vx=((float)((next_random()%1000u)/1000.0f)-0.5f)*0.32f;
    float vy=-0.16f-(float)(next_random()%1000u)/1000.0f*0.28f;
    FxParticle p={
        .position={event->screen.x+x,event->screen.y+y},
        .velocity_x=vx,.velocity_y=vy,.scale=0.58f,
        .dust_type=dust_for_biome(event->biome),.alpha=50
    };
    fx_global_emit_particle(&p);
}
