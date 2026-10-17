#include <assert.h>
#include "combo_math.h"
int main(void) {
    FxCombo c; fx_combo_reset(&c);
    for (unsigned i = 0; i < 9; ++i) {
        fx_combo_begin(&c, 0, i * 100);
        assert(c.stage == i % 3);
        fx_combo_mark_hit(&c, 2); assert(fx_combo_has_hit(&c, 2));
    }
    fx_combo_begin(&c, 0, 1000); assert(!fx_combo_has_hit(&c, 2));
    fx_combo_idle(&c, 1100); fx_combo_begin(&c, 0, 1899); assert(c.stage == 1);
    fx_combo_idle(&c, 2000); fx_combo_begin(&c, 0, 2800); assert(c.stage == 0);
    fx_combo_begin(&c, 1, 2801); assert(c.stage == 0);
    fx_combo_idle(&c, 3000); fx_combo_begin(&c, 1, 1); assert(c.stage == 0);
    assert(fx_combo_has_hit(&c, -1) && fx_combo_has_hit(&c, FX_COMBO_NPCS));
    assert(!fx_swing_active(0.0f) && !fx_swing_active(1.0f));
    assert(fx_swing_active(0.5f));
    assert(fx_swing_angle(0, 0.2f) < fx_swing_angle(0, 0.7f));
    assert(fx_swing_angle(1, 0.2f) > fx_swing_angle(1, 0.7f));
    assert(fx_swing_angle(2, 0.2f) < -1.5f);
    assert(fx_swing_progress(21,21) == 0 && fx_swing_progress(1,21) == 1);
    for (unsigned stage = 0; stage < 3; ++stage)
        for (unsigned frame = 0; frame <= 100; ++frame) {
            float a = fx_swing_angle(stage, frame / 100.0f);
            float left = fx_world_angle(a,-1), right = fx_world_angle(a,1);
            assert(fabsf(cosf(left) + cosf(right)) < 1e-5f);
            assert(fabsf(sinf(left) - sinf(right)) < 1e-5f);
        }
    FxBlade blade = {{0,0},{40,0}};
    assert(fx_blade_intersects(blade,(FxRect){20,-2,4,4},0));
    assert(!fx_blade_intersects(blade,(FxRect){20,10,4,4},2));
    assert(!fx_blade_intersects(blade,(FxRect){-20,-2,4,4},0));
    assert(!fx_blade_intersects(blade,(FxRect){45,-2,4,4},2));
    // An NPC in the broad AABB but away from the actual diagonal is rejected.
    assert(!fx_blade_intersects((FxBlade){{0,0},{40,40}},(FxRect){0,32,4,4},2));
    FxRect broad = fx_sweep_bounds(&blade,1,3);
    assert(broad.x == -3 && broad.y == -3 && broad.w == 46 && broad.h == 6);
    return 0;
}
