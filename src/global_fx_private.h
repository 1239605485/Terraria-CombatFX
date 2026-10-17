#ifndef COMBATFX_GLOBAL_FX_PRIVATE_H
#define COMBATFX_GLOBAL_FX_PRIVATE_H
#include "global_fx.h"
void fx_global_impact_emit(const FxNpcHit *event);
void fx_global_death_emit(const FxNpcHit *event);
void fx_global_ambient_emit(const FxAmbientEvent *event);
void fx_global_ambient_init(void *main_type, void *player_type);
void fx_global_ambient_tick(uint64_t now);
void fx_global_ambient_reset(void);
void fx_global_ambient_shutdown(void);
#endif
