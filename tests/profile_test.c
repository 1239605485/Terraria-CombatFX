#include <assert.h>
#include "weapon_profiles.h"
int main(void) {
    assert(fx_weapon_profile_count()==4);
    const FxWeaponProfile *wood=fx_weapon_profile_find(24);
    const FxWeaponProfile *iron=fx_weapon_profile_find(4);
    const FxWeaponProfile *shortsword=fx_weapon_profile_find(3507);
    const FxWeaponProfile *spear=fx_weapon_profile_find(280);
    assert(wood && wood->style==FX_STYLE_WOOD_COMBO);
    assert(iron && iron->style==FX_STYLE_HEAVY_SLASH && iron->reach_scale>1.0f);
    assert(shortsword && shortsword->style==FX_STYLE_SHORT_THRUST && shortsword->lunge>0);
    assert(spear && spear->style==FX_STYLE_SPEAR_THRUST && spear->reach_scale>shortsword->reach_scale);
    assert(!fx_weapon_profile_find(-1) && !fx_weapon_profile_find(999999));
    for(size_t i=0;i<fx_weapon_profile_count();++i) assert(fx_weapon_profile_at(i));
    assert(!fx_weapon_profile_at(fx_weapon_profile_count()));
    return 0;
}
