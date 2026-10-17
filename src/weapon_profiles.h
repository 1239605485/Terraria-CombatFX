#ifndef COMBATFX_WEAPON_PROFILES_H
#define COMBATFX_WEAPON_PROFILES_H
#include <stdint.h>
#include <stddef.h>

typedef enum {
    FX_STYLE_WOOD_COMBO = 0,
    FX_STYLE_HEAVY_SLASH,
    FX_STYLE_SHORT_THRUST,
    FX_STYLE_SPEAR_THRUST
} FxWeaponStyle;

typedef struct {
    int item_type;
    const char *name;
    FxWeaponStyle style;
    float reach_scale;
    float trail_width, trail_height;
    float lunge, hit_padding;
    uint8_t red, green, blue;
} FxWeaponProfile;

const FxWeaponProfile *fx_weapon_profile_find(int item_type);
size_t fx_weapon_profile_count(void);
const FxWeaponProfile *fx_weapon_profile_at(size_t index);
#endif
