#include "global_fx_private.h"
#include <math.h>

void fx_global_death_emit(const FxNpcHit *event) {
    const unsigned count=6;
    float phase=(float)((unsigned)event->npc_id%13u)*0.17f;
    for(unsigned i=0;i<count;i++) {
        float angle=phase+(2.0f*FX_PI*(float)i)/(float)count;
        float speed=(i%2u)?2.8f:2.0f;
        FxParticle p={
            .position={event->center.x+cosf(angle)*5.0f,event->center.y+sinf(angle)*5.0f},
            .velocity_x=cosf(angle)*speed,
            .velocity_y=sinf(angle)*speed-0.25f,
            .scale=(i%2u)?0.92f:0.70f,
            .dust_type=(i%2u)?27:228,
            .alpha=18
        };
        fx_global_emit_particle(&p);
    }
}
