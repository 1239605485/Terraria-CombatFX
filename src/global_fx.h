#ifndef COMBATFX_GLOBAL_FX_H
#define COMBATFX_GLOBAL_FX_H
#include <stdbool.h>
#include <stdint.h>
#include "combo_math.h"

typedef enum {
    FX_BIOME_DEFAULT=0, FX_BIOME_SNOW, FX_BIOME_HALLOW, FX_BIOME_JUNGLE,
    FX_BIOME_CORRUPTION, FX_BIOME_CRIMSON, FX_BIOME_DESERT, FX_BIOME_UNDERWORLD
} FxBiome;

typedef struct {
    FxPoint position;
    float velocity_x, velocity_y, scale;
    int dust_type, alpha;
} FxParticle;

typedef struct {
    int npc_id, damage;
    FxPoint center;
    bool critical, lethal;
    uint64_t at_ms;
} FxNpcHit;

typedef struct {
    FxPoint screen;
    int width, height;
    FxBiome biome;
    uint64_t at_ms;
} FxAmbientEvent;

typedef bool (*FxParticleSink)(const FxParticle *particle);

void fx_global_bind(FxParticleSink sink);
void fx_global_init(void *main_type, void *player_type);
void fx_global_reset(void);
void fx_global_shutdown(void);
bool fx_global_npc_hit(const FxNpcHit *event);
void fx_global_ambient(const FxAmbientEvent *event);
void fx_global_tick(uint64_t now);
bool fx_global_emit_particle(const FxParticle *particle);
#endif
