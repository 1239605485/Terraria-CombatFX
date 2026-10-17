#include "weapon_profiles.h"
#include <stddef.h>

// Terraria 1.4.5.8.6 ItemID values. Keep per-weapon tuning here so the
// shared hook engine never grows a chain of item-type conditionals.
static const FxWeaponProfile profiles[] = {
    {24,   "Wooden Sword",      FX_STYLE_WOOD_COMBO,    1.00f, 1.28f, 0.86f,  0.0f, 3.0f, 255, 248, 224},
    {4,    "Iron Broadsword",   FX_STYLE_HEAVY_SLASH,   1.12f, 1.50f, 1.40f,  0.0f, 4.0f, 255, 151,  48},
    {3507, "Copper Shortsword", FX_STYLE_SHORT_THRUST,  0.88f, 0.94f, 0.52f, 10.0f, 2.4f,  56, 225, 255},
    {280,  "Spear",             FX_STYLE_SPEAR_THRUST,  1.38f, 1.30f, 0.70f, 18.0f, 3.0f,  94, 142, 255},
};

const FxWeaponProfile *fx_weapon_profile_find(int item_type) {
    for (size_t i = 0; i < sizeof(profiles) / sizeof(profiles[0]); ++i)
        if (profiles[i].item_type == item_type) return &profiles[i];
    return NULL;
}
size_t fx_weapon_profile_count(void) { return sizeof(profiles) / sizeof(profiles[0]); }
const FxWeaponProfile *fx_weapon_profile_at(size_t index) {
    return index < fx_weapon_profile_count() ? &profiles[index] : NULL;
}
