#include "global_fx_private.h"
#include "effect_budget.h"

static FxParticleSink particle_sink;
static FxBudget hit_budget;
static uint64_t ambient_last;
static bool ambient_seen;

void fx_global_bind(FxParticleSink sink) { particle_sink=sink; }
bool fx_global_emit_particle(const FxParticle *particle) {
    return particle_sink && particle && particle_sink(particle);
}
void fx_global_init(void *main_type, void *player_type) {
    fx_global_reset();
    fx_global_ambient_init(main_type,player_type);
}
void fx_global_reset(void) {
    fx_budget_reset(&hit_budget);
    ambient_last=0; ambient_seen=false;
    fx_global_ambient_reset();
}
void fx_global_shutdown(void) {
    fx_global_ambient_shutdown();
    fx_global_reset();
    particle_sink=NULL;
}
bool fx_global_npc_hit(const FxNpcHit *event) {
    if (!event || event->npc_id<0 || !particle_sink) return false;
    unsigned count=(event->critical ? 6u : 4u)+(event->lethal ? 6u : 0u);
    if (!fx_budget_take(&hit_budget,event->at_ms,event->npc_id,count)) return false;
    fx_global_impact_emit(event);
    if(event->lethal) fx_global_death_emit(event);
    return true;
}
void fx_global_ambient(const FxAmbientEvent *event) {
    if(!event || event->width<=0 || event->height<=0) return;
    if(ambient_seen && event->at_ms>=ambient_last && event->at_ms-ambient_last<850) return;
    if(ambient_seen && event->at_ms<ambient_last) ambient_seen=false;
    ambient_seen=true; ambient_last=event->at_ms;
    fx_global_ambient_emit(event);
}
void fx_global_tick(uint64_t now) { fx_global_ambient_tick(now); }
