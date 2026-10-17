#define _POSIX_C_SOURCE 200809L
#include <assert.h>
#include "../src/combatfx.c"
static int player_object, other_object, item_object, npc_object, array_object;
static Vec2 center_get(void *p, const void *m) { (void)p; (void)m; return (Vec2){100,100}; }
static int item_type = 24, animation = 21, maximum = 21, slot_number, facing = 1, mode;
static Vec2 item_location;
static float item_rotation;
static FxRect npc_box;
static int draw_object, cache_object, cache_count;
static Vec2 draw_position = {90,80}, draw_location = {4,5};
static float live_scale = 1, draw_arm;
static WoodDrawData cache_data[16];
static int arm_calls;
static patch_handle_t fake(unsigned n) { return (void *)(uintptr_t)n; }
static int local(const void *m) { (void)m; return 0; }
static bool menu(const void *m) { (void)m; return false; }
static void *held(void *p, const void *m) { (void)p; (void)m; return &item_object; }
static int slot(void *p, const void *m) { (void)p; (void)m; return slot_number; }
static void arm(void *p, bool enable, int stretch, float rotation, const void *m) {
    (void)p; (void)m; assert(enable && stretch == 0 && isfinite(rotation)); ++arm_calls;
}
static Vec2 hand(void *p, int stretch, float rotation, const void *m) {
    (void)p; (void)m; assert(stretch == 0);
    return (Vec2){100 + cosf(rotation + FX_PI/2) * 10, 100 + sinf(rotation + FX_PI/2) * 10};
}
static float scale(void *p, void *i, const void *m) { (void)p; (void)i; (void)m; return 1; }
static FxRect hitbox(void *p, const void *m) { (void)p; (void)m; return npc_box; }
static size_t array_length(patch_handle_t p) { if (p == &cache_object) return 16; assert(p == &array_object); return 200; }
static bool array_at(patch_handle_t p, size_t i, void *out) {
    if (p == &cache_object) { if (i >= 16) return false; memcpy(out,&cache_data[i],sizeof(WoodDrawData)); return true; }
    assert(p == &array_object); if (i >= 200) return false; *(void **)out = &npc_object; return true;
}
static void *pointer(patch_handle_t f, void *p) {
    if (p == &draw_object) {
        if (f == wood_view.location) return &draw_location;
        if (f == wood_view.position) return &draw_position;
        return NULL;
    }
    assert(p == &player_object); return f == wood.location ? &item_location : NULL;
}
static void set(patch_handle_t f, patch_handle_t p, void *v) {
    if (f == wood.rotation) { assert(p == &player_object); memcpy(&item_rotation,v,4); }
    else if (f == wood.animation) { assert(p == &player_object); memcpy(&animation,v,4); }
    else if (f == wood_view.item_scale) { assert(p == &item_object); memcpy(&live_scale,v,4); }
    else if (f == wood_view.arm_rotation) { assert(p == &draw_object); memcpy(&draw_arm,v,4); }
    else assert(false);
}
static void get(patch_handle_t f, patch_handle_t p, void *out) {
    if (f == f_id) *(int *)out = p == &other_object ? 1 : 0;
    else if (f == f_netmode) *(int *)out = mode;
    else if (f == wood.type) *(int *)out = item_type;
    else if (f == wood.animation) *(int *)out = animation;
    else if (f == wood.maximum) *(int *)out = maximum;
    else if (f == wood.rotation) *(float *)out = item_rotation;
    else if (f == wood.direction) *(int *)out = facing;
    else if (f == wood.gravity) *(float *)out = 1;
    else if (f == wood.dead || f == wood.frozen) *(bool *)out = false;
    else if (f == wood.npcs) *(void **)out = &array_object;
    else if (f == wood_view.player) *(void **)out = &player_object;
    else if (f == wood_view.item_scale) *(float *)out = live_scale;
    else if (f == wood_view.arm_rotation) *(float *)out = draw_arm;
    else if (f == wood_view.shadow) *(float *)out = 0;
    else if (f == wood_view.count) *(int *)out = cache_count;
    else if (f == wood_view.cache) *(void **)out = &cache_object;
    else if (f == wood_view.player_width) *(int *)out = 20;
    else if (f == wood_view.player_height) *(int *)out = 40;
    else assert(false);
}
static void set_draw(void *entry, void *texture, Vec2 pos, Color color, float rotation, Vec2 origin, Vec2 scale, int effect, int depth, const void *m) {
    (void)depth; (void)m;
    WoodDrawData *d = entry; memset(d,0,sizeof(*d));
    d->texture = texture; d->position = pos; d->color = color; d->rotation = rotation;
    d->origin = origin; d->scale = scale; d->effect = effect;
}
static void *add_draw(void *draw, const void *m) {
    (void)m; assert(draw == &draw_object && cache_count < 16); return &cache_data[cache_count++];
}
static void original_draw(void *draw, const void *m) {
    (void)m; assert(draw == &draw_object && animation > 0);
    WoodDrawData *data = add_draw(draw,NULL);
    set_draw(data,&item_object,draw_location,(Color){255,255,255,255},item_rotation,(Vec2){0,32},(Vec2){live_scale,live_scale},0,0,NULL);
}
static int processed;
static void original_process(void *player, void *item, FxRect rect, int damage, float kb, int index, const void *info) {
    (void)player; (void)item; (void)kb; (void)info;
    assert(rect.w > 0 && index == 0); ++processed; wood_record_hit(&npc_object, &damage);
}
static void original_box(void *player, void *item, FxRect frame, bool *dont, FxRect *box, const void *info) {
    (void)player; (void)item; (void)info; assert(frame.w == 32 && frame.h == 32);
    *dont = false; *box = (FxRect){1,2,3,4};
}
int main(void) {
    f_id = fake(1); f_netmode = fake(2);
    wood.type = fake(3); wood.animation = fake(4); wood.maximum = fake(5);
    wood.direction = fake(6); wood.gravity = fake(7); wood.dead = fake(8); wood.frozen = fake(9);
    wood.location = fake(10); wood.rotation = fake(11); wood.npcs = fake(12);
    get_center = center_get; get_player = local; get_menu = menu;
    patchlib_field_get_value = get; patchlib_field_set_value = set; patchlib_field_get_pointer = pointer;
    patchlib_array_length = array_length; patchlib_array_at = array_at;
    wood.held = held; wood.slot = slot; wood.arm = arm; wood.hand = hand; wood.scale = scale; wood.hitbox = hitbox;
    wood.ready = true; fx_combo_reset(&wood.combo);
    void *item = &item_object; void *begin_args[] = {&item};
    wood_begin(&player_object,begin_args,NULL,NULL); assert(wood.combo.stage == 0);
    FxRect frame = {0,0,32,32}, box = {0}; bool dont_attack = false;
    bool *dont_pointer = &dont_attack; FxRect *box_pointer = &box;
    void *box_args[] = {&item,&frame,&dont_pointer,&box_pointer};
    wood_get_hitbox(&player_object,box_args,NULL,NULL); assert(dont_attack && box.w == 0);
    animation = 11;
    wood_get_hitbox(&player_object,box_args,NULL,NULL);
    assert(!dont_attack && box.w > 0 && box.h > 0 && arm_calls > 0);
    assert(isfinite(item_rotation) && isfinite(item_location.x));
    int damage = 7, index = 0; float knockback = 4;
    void *process_args[] = {&item,&box,&damage,&knockback,&index};
    npc_box = (FxRect){(int)wood.blades[6].tip.x-4,(int)wood.blades[6].tip.y-4,8,8};
    assert(!wood_process_prefix(&player_object,process_args,NULL,NULL));
    assert(wood_process_npc == 0);
    int actual = 0; wood_record_hit(&npc_object,&actual); assert(!fx_combo_has_hit(&wood.combo,0));
    actual = 7; wood_record_hit(&npc_object,&actual); assert(fx_combo_has_hit(&wood.combo,0));
    wood_process_postfix(NULL,NULL,NULL,NULL); assert(wood_process_npc == -1);
    assert(wood_process_prefix(&player_object,process_args,NULL,NULL));
    wood_process_postfix(NULL,NULL,NULL,NULL);
    // Next swing clears target hit history; misses do not consume the target.
    animation = 21; wood_begin(&player_object,begin_args,NULL,NULL); assert(wood.combo.stage == 1);
    animation = 11; wood_get_hitbox(&player_object,box_args,NULL,NULL);
    npc_box = (FxRect){1000,1000,10,10};
    assert(wood_process_prefix(&player_object,process_args,NULL,NULL));
    wood_process_postfix(NULL,NULL,NULL,NULL); assert(!fx_combo_has_hit(&wood.combo,0));
    animation = 21; wood_begin(&player_object,begin_args,NULL,NULL); assert(wood.combo.stage == 2);
    wood_begin(&player_object,begin_args,NULL,NULL); assert(wood.combo.stage == 0);
    // Typed Rectangle and out-reference wrappers preserve ABI-shaped arguments.
    wood.original_box = original_box; wood.original_process = original_process;
    animation = 11;
    wood_native_box(&player_object, &item_object, frame, &dont_attack, &box, NULL);
    assert(box.w > 4 && !dont_attack);
    npc_box = (FxRect){(int)wood.blades[6].tip.x-4,(int)wood.blades[6].tip.y-4,8,8};
    wood_native_process(&player_object,&item_object,box,7,4,0,NULL);
    wood_native_process(&player_object,&item_object,box,7,4,0,NULL);
    assert(processed == 1 && wood_process_depth == 0 && wood_process_npc == -1);
    // Another player and another weapon retain the caller's original hitbox.
    box = (FxRect){1,2,3,4}; dont_attack = false;
    wood_get_hitbox(&other_object,box_args,NULL,NULL); assert(box.x == 1 && !dont_attack);
    item_type = 25; wood_get_hitbox(&player_object,box_args,NULL,NULL); assert(box.x == 1 && !dont_attack);
    wood_watch(&player_object,NULL,NULL,NULL); assert(!wood.combo.valid);
    item_type = 24; wood_begin(&player_object,begin_args,NULL,NULL); assert(wood.combo.stage == 0);
    ++slot_number; wood_watch(&player_object,NULL,NULL,NULL); assert(!wood.combo.valid);
    mode = 1; wood_begin(&player_object,begin_args,NULL,NULL); assert(!wood.combo.valid);
    // Rendering-only animation and scale changes must be restored, including idle sword display.
    mode = 0; wood_view.ready = true; wood_view.original = original_draw;
    wood_view.add = add_draw; wood_view.set = set_draw;
    wood_view.player = fake(20); wood_view.item_scale = fake(21); wood_view.arm_rotation = fake(22);
    wood_view.shadow = fake(23); wood_view.count = fake(24); wood_view.cache = fake(25);
    wood_view.location = fake(26); wood_view.position = fake(27);
    wood_view.player_width = fake(28); wood_view.player_height = fake(29);
    animation = 0; item_rotation = 0.23f; item_location = (Vec2){2,3};
    fx_combo_reset(&wood.combo); cache_count = 0;
    wood_native_draw(&draw_object,NULL);
    assert(animation == 0 && live_scale == 1 && item_rotation == 0.23f);
    assert(item_location.x == 2 && item_location.y == 3 && draw_location.x == 4 && draw_location.y == 5);
    assert(cache_count == 1 && fabsf(cache_data[0].scale.x - 0.55f) < 0.0001f);
    assert(cache_data[0].position.x == 105 && cache_data[0].position.y == 105);
    // Animation transition can start/progress a combo even when an auto-use path skips StartActualUse.
    animation = 21; assert(wood_pose(&player_object,&item_object)); assert(wood.combo.stage == 0);
    assert(fabsf(wood.visual_rotation - (2.55f + FX_PI*0.25f)) < 0.0001f);
    assert(wood.visual_stretch == 0.55f && wood.visual_hand.x == 105 && wood.visual_hand.y == 105);
    animation = 11; cache_count = 0; wood_native_draw(&draw_object,NULL);
    assert(animation == 11 && live_scale == 1 && cache_count == 1);
    animation = 10; cache_count = 0; wood_native_draw(&draw_object,NULL);
    assert(animation == 10 && live_scale == 1 && cache_count == 2);
    assert(cache_data[1].color.a < 100 && wood_view.trail_count <= 3);
    animation = 21; assert(wood_pose(&player_object,&item_object)); assert(wood.combo.stage == 1);
    animation = 11; assert(wood_pose(&player_object,&item_object));
    animation = 21; assert(wood_pose(&player_object,&item_object)); assert(wood.combo.stage == 2);
    assert(wood_process_depth == 0);
    return 0;
}
