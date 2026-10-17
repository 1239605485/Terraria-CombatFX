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
    assert(fx_swing_fraction(0,0.22f)==0 && fx_swing_fraction(0,0.82f)==1);
    assert(fx_swing_fraction(1,0.22f)==0);
    assert(fx_swing_fraction(2,0.82f)==1);
    for(unsigned stage=0;stage<3;stage++) {
        assert(fx_swing_fraction(stage,0.52f)>0.45f && fx_swing_fraction(stage,0.52f)<0.55f);
    }
    assert(fx_swing_angle(0, 0.2f) < fx_swing_angle(0, 0.7f));
    assert(fx_swing_angle(1, 0.2f) < fx_swing_angle(1, 0.7f));
    assert(fx_swing_angle(2, 0.2f) > fx_swing_angle(2, 0.7f));
    for (unsigned i=0;i<=100;++i) assert(cosf(fx_swing_angle(0,i/100.0f)) > 0);
    assert(fx_swing_progress(21,21) == 0 && fx_swing_progress(1,21) == 1);
    for (unsigned stage = 0; stage < 3; ++stage)
        for (unsigned frame = 0; frame <= 100; ++frame) {
            float a = fx_swing_angle(stage, frame / 100.0f);
            float left = fx_world_angle(a,-1), right = fx_world_angle(a,1);
            assert(fabsf(cosf(left) + cosf(right)) < 1e-5f);
            assert(fabsf(sinf(left) - sinf(right)) < 1e-5f);
        }
    for (unsigned i = 0; i < 100; ++i) {
        float a = -FX_PI + i * 2 * FX_PI / 100;
        float projected = fx_project_angle(0,a), length = fx_project_length(0,a);
        assert(fabsf(cosf(projected)*length-cosf(a)) < 1e-5f);
        assert(fabsf(sinf(projected)*length-sinf(a)*0.30f) < 1e-5f);
        assert(fx_project_length(2,a) == 1);
    }
    assert(fx_draw_lift(0) == 0 && fx_draw_lift(0.22f) == 1);
    // Every stage's recovery must meet the next entry without a pose/scale jump.
    for(unsigned stage=0;stage<3;stage++) {
        FxPose end=fx_pose(stage,1), start=fx_pose((stage+1)%3,0);
        assert(fabsf(cosf(end.angle)-cosf(start.angle))<1e-5f);
        assert(fabsf(sinf(end.angle)-sinf(start.angle))<1e-5f);
        assert(fabsf(end.reach-start.reach)<1e-5f);
        assert(fabsf(end.forward-start.forward)<1e-5f && fabsf(end.hip-start.hip)<1e-5f);
        FxPose previous=fx_pose(stage,0);
        for(int frame=1;frame<=1000;frame++) {
            FxPose current=fx_pose(stage,frame/1000.0f);
            assert(fabsf(current.angle-previous.angle)<0.08f);
            previous=current;
        }
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
