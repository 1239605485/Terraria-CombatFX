#include "global_fx_private.h"
#include <math.h>

void fx_global_impact_emit(const FxNpcHit *event) {
    unsigned count=event->critical?6u:4u;
    float radius=event->critical?11.5f:7.0f;
    float speed=event->critical?3.0f:2.1f;
    float phase=(float)((unsigned)event->npc_id%17u)*0.11f;
    for(unsigned i=0;i<count;i++) {
        float angle=phase+(2.0f*FX_PI*(float)i)/(float)count;
        FxParticle p={
            .position={event->center.x+cosf(angle)*radius,event->center.y+sinf(angle)*radius},
            .velocity_x=cosf(angle)*speed,
            .velocity_y=sinf(angle)*speed,
            .scale=event->critical?1.0f:0.72f,
            .dust_type=228,
            .alpha=24
        };
        fx_global_emit_particle(&p);
    }
}
