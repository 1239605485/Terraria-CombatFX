// Included after the base helpers; no retained game objects.
#include "dobby.h"
typedef void (*WoodStartOriginal)(void *, void *, const void *);
typedef void (*WoodStyleOriginal)(void *, float, void *, FxRect, const void *);
typedef void (*WoodBoxOriginal)(void *, void *, FxRect, bool *, FxRect *, const void *);
typedef void (*WoodProcessOriginal)(void *, void *, FxRect, int, float, int, const void *);
typedef void (*WoodTickOriginal)(void *, const void *);
typedef void *(*WoodHeld)(void *, const void *);
typedef int (*WoodSlot)(void *, const void *);
typedef void (*WoodArm)(void *, bool, int, float, const void *);
typedef Vec2 (*WoodHand)(void *, int, float, const void *);
typedef float (*WoodScale)(void *, void *, const void *);
typedef FxRect (*WoodHitbox)(void *, const void *);
static struct {
    bool ready, sample_active, logged;
    unsigned sequence_logs;
    int observed_animation;
    float visual_rotation, visual_stretch, visual_progress, arm_rotation;
    Vec2 visual_hand;
    FxCombo combo;
    FxBlade blades[FX_SWEEP_SAMPLES];
    float length, padding;
    GetBool inventory_open;
    patch_handle_t minventory;
    bool ui_logged;
    WoodHeld held;
    WoodSlot slot;
    WoodArm arm;
    WoodHand hand;
    WoodScale scale;
    WoodHitbox hitbox;
    patch_handle_t mh, ms, ma, mhand, mscale, mbox;
    patch_handle_t type, animation, maximum, rotation, location, direction, gravity, dead, frozen, npcs;
    void *addresses[6];
    WoodStartOriginal original_start;
    WoodStyleOriginal original_style;
    WoodBoxOriginal original_box;
    WoodProcessOriginal original_process;
    WoodTickOriginal original_watch, original_frame;
    unsigned hook_count;
} wood;
static _Thread_local int wood_process_npc = -1;
static _Thread_local int wood_process_stack[16];
static _Thread_local unsigned wood_process_depth;

static bool wood_local(patch_handle_t player) {
    if (!wood.ready || !player || !playing()) return false;
    int mode = 2, id = -1;
    patchlib_field_get_value(f_netmode, NULL, &mode);
    patchlib_field_get_value(f_id, player, &id);
    return mode == 0 && id == get_player(m_player);
}
static bool wood_item(patch_handle_t item) {
    int type = -1;
    if (item) patchlib_field_get_value(wood.type, item, &type);
    return type == FX_WOODEN_SWORD;
}
static bool wood_can_animate(patch_handle_t player, patch_handle_t item) {
    if (!wood_local(player) || !wood_item(item)) return false;
    // Crafting shares live item instances: never override weapon pose/scale while inventory UI owns them.
    if (!wood.inventory_open || wood.inventory_open(wood.minventory)) {
        if (!wood.ui_logged) { note("Wood overrides bypassed: inventory/crafting open or state unavailable."); wood.ui_logged=true; }
        wood.sample_active=false; fx_combo_reset(&wood.combo); return false;
    }
    wood.ui_logged=false;
    float gravity = 0; bool dead = true, frozen = true;
    patchlib_field_get_value(wood.gravity, player, &gravity);
    patchlib_field_get_value(wood.dead, player, &dead);
    patchlib_field_get_value(wood.frozen, player, &frozen);
    return gravity > 0 && !dead && !frozen;
}
static void wood_draw_init(patch_handle_t main_type, patch_handle_t player, patch_handle_t entity);
static void wood_draw_cleanup(void);
static void wood_start(int slot, const char *reason) {
    fx_combo_begin(&wood.combo, slot, now_ms());
    wood.observed_animation = -1;
    if (wood.sequence_logs < 9) {
        char line[140];
        snprintf(line,sizeof(line),"Wood combo stage=%u slot=%d trigger=%s",wood.combo.stage+1,slot,reason);
        note(line); ++wood.sequence_logs;
    }
}
static bool wood_pose(patch_handle_t player, patch_handle_t item) {
    wood.sample_active = false;
    if (!wood_can_animate(player, item)) return false;
    int animation = 0, maximum = 0, direction = 0;
    patchlib_field_get_value(wood.animation, player, &animation);
    patchlib_field_get_value(wood.maximum, player, &maximum);
    patchlib_field_get_value(wood.direction, player, &direction);
    if (animation <= 0 || maximum <= 1 || maximum > 3600 || (direction != -1 && direction != 1)) return false;
    int slot = wood.slot(player, wood.ms);
    if (wood.combo.valid && wood.combo.slot != slot) fx_combo_reset(&wood.combo);
    if (!wood.combo.running || (wood.observed_animation >= 0 && animation > wood.observed_animation + 1))
        wood_start(slot, "animation-transition");
    wood.observed_animation = animation;
    float progress = fx_swing_progress(animation, maximum);
    float scale = wood.scale(player, item, wood.mscale);
    if (!isfinite(scale) || scale <= 0 || scale > 4) return false;
    FxPose pose=fx_pose(wood.combo.stage,progress);
    float world_angle=fx_world_angle(pose.angle,direction);
    float arm_angle=world_angle-FX_PI*0.5f;
    wood.arm(player,true,0,arm_angle,wood.ma);
    Vec2 hand=wood.hand(player,0,arm_angle,wood.mhand);
    if (!isfinite(hand.x) || !isfinite(hand.y) || fabsf(hand.x)>1e8f || fabsf(hand.y)>1e8f) return false;
    Vec2 center=get_center(player,m_center);
    Vec2 hip={center.x+direction*5,center.y+5};
    hand.x+=(hip.x-hand.x)*pose.hip+direction*pose.forward;
    hand.y+=(hip.y-hand.y)*pose.hip;
    float rotation=direction*(pose.angle+FX_PI*0.25f);
    wood.visual_hand=hand; wood.visual_rotation=rotation; wood.visual_progress=progress;
    wood.arm_rotation=arm_angle; wood.visual_stretch=pose.reach;
    void *location = patchlib_field_get_pointer(wood.location, player);
    if (!location) return false;
    // The supplied kernel reports sizeof(void*) for value-type fields on arm32.
    memcpy(location, &hand, sizeof(hand));
    patchlib_field_set_value(wood.rotation, player, &rotation);
    wood.padding = 3.0f * scale;
    float previous = fmaxf(0.16f, progress - 1.0f / (float)(maximum - 1));
    for (unsigned i = 0; i < FX_SWEEP_SAMPLES; ++i) {
        float t = previous + (progress - previous) * (float)i / (FX_SWEEP_SAMPLES - 1);
        FxPose sample=fx_pose(wood.combo.stage,t);
        float angle=fx_world_angle(sample.angle,direction);
        float reach=wood.length*scale*sample.reach;
        Vec2 h = wood.hand(player, 0, angle - FX_PI * 0.5f, wood.mhand);
        if (!isfinite(h.x) || !isfinite(h.y) || fabsf(h.x) > 1e8f || fabsf(h.y) > 1e8f) return false;
        h.x+=(hip.x-h.x)*sample.hip+direction*sample.forward;
        h.y+=(hip.y-h.y)*sample.hip;
        wood.blades[i].hand = h;
        wood.blades[i].tip = (Vec2){h.x + cosf(angle) * reach, h.y + sinf(angle) * reach};
    }
    float cut=fx_swing_fraction(wood.combo.stage,progress);
    wood.sample_active = cut>0 && cut<1;
    return true;
}
static void wood_begin(patch_handle_t player, void **args, void *result, const patch_method_signature_t *sig) {
    (void)result; (void)sig;
    if (!args || !args[0]) return;
    patch_handle_t item = *(void **)args[0];
    if (!wood_local(player)) return;
    if (!wood_can_animate(player, item)) { fx_combo_reset(&wood.combo); return; }
    wood_start(wood.slot(player, wood.ms), "actual-use");
    wood.length = 36;
    if (!wood.logged) { wood.logged = true; note("Wooden sword combo started: forward / downward / upward; single-player only."); }
}
static void wood_use_style(patch_handle_t player, void **args, void *result, const patch_method_signature_t *sig) {
    (void)result; (void)sig;
    if (!args || !args[1] || !args[2]) return;
    if (!wood_local(player)) return;
    FxRect frame; memcpy(&frame, args[2], sizeof(frame));
    if (frame.w > 0 && frame.h > 0 && frame.w <= 256 && frame.h <= 256)
        wood.length = fx_clamp(hypotf((float)frame.w, (float)frame.h) - 6, 16, 100);
    wood_pose(player, *(void **)args[1]);
}
static void wood_get_hitbox(patch_handle_t player, void **args, void *result, const patch_method_signature_t *sig) {
    (void)result; (void)sig;
    if (!args || !args[0] || !args[1] || !args[2] || !args[3]) return;
    if (!wood_local(player)) return;
    FxRect frame; memcpy(&frame, args[1], sizeof(frame));
    if (frame.w > 0 && frame.h > 0 && frame.w <= 256 && frame.h <= 256)
        wood.length = fx_clamp(hypotf((float)frame.w, (float)frame.h) - 6, 16, 100);
    if (!wood_pose(player, *(void **)args[0])) return;
    bool *dont_attack = *(bool **)args[2];
    FxRect *box = *(FxRect **)args[3];
    if (!dont_attack || !box) return;
    *dont_attack = !wood.sample_active;
    *box = wood.sample_active ? fx_sweep_bounds(wood.blades, FX_SWEEP_SAMPLES, wood.padding) : (FxRect){0,0,0,0};
}
static bool wood_process_prefix(patch_handle_t player, void **args, const patch_method_signature_t *sig, void *result) {
    (void)sig; (void)result;
    // A postfix also runs when the original is skipped; balance every frame.
    if (wood_process_depth < 16) wood_process_stack[wood_process_depth] = wood_process_npc;
    ++wood_process_depth;
    wood_process_npc = -1;
    if (!wood_local(player) || !args || !args[0] || !args[4]) return false;
    patch_handle_t item = *(void **)args[0];
    if (!wood_can_animate(player, item) || !wood.combo.running) return false;
    int index = *(int *)args[4];
    if (!wood.sample_active || fx_combo_has_hit(&wood.combo, index)) return true;
    patch_handle_t array = NULL, npc = NULL;
    patchlib_field_get_value(wood.npcs, NULL, &array);
    if (!array || index < 0 || (size_t)index >= patchlib_array_length(array) || !patchlib_array_at(array, (size_t)index, &npc) || !npc) return true;
    FxRect target = wood.hitbox(npc, wood.mbox);
    bool intersects = false;
    for (unsigned i = 0; i < FX_SWEEP_SAMPLES && !intersects; ++i)
        intersects = fx_blade_intersects(wood.blades[i], target, wood.padding);
    if (!intersects) return true;
    wood_process_npc = index;
    return false;
}
static void wood_process_postfix(patch_handle_t player, void **args, void *result, const patch_method_signature_t *sig) {
    (void)player; (void)args; (void)result; (void)sig;
    if (wood_process_depth) --wood_process_depth;
    wood_process_npc = wood_process_depth < 16 ? wood_process_stack[wood_process_depth] : -1;
}
static void wood_record_hit(patch_handle_t npc, void *result) {
    if (!wood.ready || !wood.combo.running || wood_process_npc < 0 || !npc || !result || *(int *)result <= 0) return;
    int index = -1; patchlib_field_get_value(f_id, npc, &index);
    if (index == wood_process_npc) fx_combo_mark_hit(&wood.combo, index);
}
static void wood_watch(patch_handle_t player, void **args, void *result, const patch_method_signature_t *sig) {
    (void)args; (void)result; (void)sig;
    if (!wood.ready || !player) return;
    int id = -1; patchlib_field_get_value(f_id, player, &id);
    if (id != get_player(m_player)) return;
    patch_handle_t item = wood.held(player, wood.mh);
    if (!wood_can_animate(player, item) || (wood.combo.valid && wood.slot(player, wood.ms) != wood.combo.slot)) {
        fx_combo_reset(&wood.combo); wood.sample_active = false; return;
    }
    int animation = 0; patchlib_field_get_value(wood.animation, player, &animation);
    if (animation <= 0) { fx_combo_idle(&wood.combo, now_ms()); wood.sample_active = false; wood.observed_animation = 0; }
}
static void wood_frame(patch_handle_t player, void **args, void *result, const patch_method_signature_t *sig) {
    (void)args; (void)result; (void)sig;
    if (wood_local(player)) wood_pose(player, wood.held(player, wood.mh));
}
// Typed native wrappers keep Rectangle's four ints out of kernel FFI conversion.
static void wood_native_start(void *player, void *item, const void *info) {
    wood.original_start(player, item, info);
    void *args[] = {&item}; wood_begin(player, args, NULL, NULL);
}
static void wood_native_style(void *player, float mount, void *item, FxRect frame, const void *info) {
    wood.original_style(player, mount, item, frame, info);
    void *args[] = {&mount, &item, &frame}; wood_use_style(player, args, NULL, NULL);
}
static void wood_native_box(void *player, void *item, FxRect frame, bool *dont_attack, FxRect *box, const void *info) {
    wood.original_box(player, item, frame, dont_attack, box, info);
    void *args[] = {&item, &frame, &dont_attack, &box}; wood_get_hitbox(player, args, NULL, NULL);
}
static void wood_native_process(void *player, void *item, FxRect box, int damage, float knockback, int index, const void *info) {
    void *args[] = {&item, &box, &damage, &knockback, &index};
    if (!wood_process_prefix(player, args, NULL, NULL))
        wood.original_process(player, item, box, damage, knockback, index, info);
    wood_process_postfix(player, args, NULL, NULL);
}
static void wood_native_watch(void *player, const void *info) {
    wood.original_watch(player, info); wood_watch(player, NULL, NULL, NULL);
}
static void wood_native_frame(void *player, const void *info) {
    wood.original_frame(player, info); wood_frame(player, NULL, NULL, NULL);
}
static void wood_cleanup(void) {
    wood_draw_cleanup();
    wood.ready = false;
    for (unsigned i = wood.hook_count; i > 0; --i) DobbyDestroy(wood.addresses[i-1]);
    memset(&wood, 0, sizeof(wood)); fx_combo_reset(&wood.combo);
    wood_process_npc = -1; wood_process_depth = 0;
}
static void wood_init(patch_handle_t main_type, patch_handle_t entity) {
    fx_combo_reset(&wood.combo);
    if (!patchlib_field_get_pointer || !patchlib_field_set_value || !patchlib_array_at || !patchlib_array_length) {
        note("Wood combo disabled: extra kernel APIs unavailable."); return;
    }
    patch_handle_t player = patchlib_type_get_type("Terraria", "Player");
    patch_handle_t item = patchlib_type_get_type("Terraria", "Item");
    wood.type = field(item, "type", sizeof(int), false);
    wood.animation = field(player, "itemAnimation", sizeof(int), false);
    wood.maximum = field(player, "itemAnimationMax", sizeof(int), false);
    wood.rotation = field(player, "itemRotation", sizeof(float), false);
    wood.location = player ? patchlib_type_get_field(player, "itemLocation") : NULL;
    wood.direction = field(entity, "direction", sizeof(int), false);
    wood.gravity = field(player, "gravDir", sizeof(float), false);
    wood.dead = field(player, "dead", sizeof(bool), false);
    wood.frozen = field(player, "frozen", sizeof(bool), false);
    wood.npcs = field(main_type, "npc", sizeof(void *), true);
    wood.minventory = method(main_type,"get_playerInventory",false,PATCH_BOOL,NULL,0);
    wood.inventory_open = wood.minventory ? (GetBool)patchlib_method_get_pointer(wood.minventory) : NULL;
    const patch_type_t arm_args[] = {PATCH_BOOL, PATCH_INT32, PATCH_FLOAT};
    const patch_type_t hand_args[] = {PATCH_INT32, PATCH_FLOAT};
    const patch_type_t item_arg[] = {PATCH_OBJECT};
    wood.mh = method(player, "get_HeldItem", true, PATCH_OBJECT, NULL, 0);
    wood.ms = method(player, "get_selectedItem", true, PATCH_INT32, NULL, 0);
    wood.ma = method(player, "SetCompositeArmFront", true, PATCH_VOID, arm_args, 3);
    wood.mhand = method(player, "GetFrontHandPosition", true, PATCH_STRUCT, hand_args, 2);
    wood.mscale = method(player, "GetAdjustedItemScale", true, PATCH_FLOAT, item_arg, 1);
    wood.mbox = method(entity, "get_Hitbox", true, PATCH_STRUCT, NULL, 0);
    if (!wood.inventory_open || !wood.type || !wood.animation || !wood.maximum || !wood.rotation || !wood.location ||
        patchlib_field_is_static(wood.location) || !wood.direction || !wood.gravity || !wood.dead || !wood.frozen || !wood.npcs ||
        !wood.mh || !wood.ms || !wood.ma || !wood.mhand || !wood.mscale || !wood.mbox) {
        note("Wood combo disabled: player field or method mismatch."); return;
    }
    wood.held = (WoodHeld)patchlib_method_get_pointer(wood.mh);
    wood.slot = (WoodSlot)patchlib_method_get_pointer(wood.ms);
    wood.arm = (WoodArm)patchlib_method_get_pointer(wood.ma);
    wood.hand = (WoodHand)patchlib_method_get_pointer(wood.mhand);
    wood.scale = (WoodScale)patchlib_method_get_pointer(wood.mscale);
    wood.hitbox = (WoodHitbox)patchlib_method_get_pointer(wood.mbox);
    if (!wood.held || !wood.slot || !wood.arm || !wood.hand || !wood.scale || !wood.hitbox) {
        note("Wood combo disabled: native pointers unavailable."); return;
    }
    const patch_type_t style_args[] = {PATCH_FLOAT, PATCH_OBJECT, PATCH_STRUCT};
    const patch_type_t box_args[] = {PATCH_OBJECT, PATCH_STRUCT, PATCH_POINTER, PATCH_POINTER};
    const patch_type_t process_args[] = {PATCH_OBJECT, PATCH_STRUCT, PATCH_INT32, PATCH_FLOAT, PATCH_INT32};
    patch_handle_t methods[] = {
        method(player, "ItemCheck_StartActualUse", true, PATCH_VOID, item_arg, 1),
        method(player, "ItemCheck_ApplyUseStyle", true, PATCH_VOID, style_args, 3),
        method(player, "ItemCheck_GetMeleeHitbox", true, PATCH_VOID, box_args, 4),
        method(player, "ProcessHitAgainstNPC", true, PATCH_VOID, process_args, 5),
        method(player, "ItemCheck", true, PATCH_VOID, NULL, 0),
        method(player, "PlayerFrame", true, PATCH_VOID, NULL, 0)
    };
    void *replacements[] = {(void *)wood_native_start, (void *)wood_native_style, (void *)wood_native_box,
                            (void *)wood_native_process, (void *)wood_native_watch, (void *)wood_native_frame};
    void **originals[] = {(void **)&wood.original_start, (void **)&wood.original_style, (void **)&wood.original_box,
                          (void **)&wood.original_process, (void **)&wood.original_watch, (void **)&wood.original_frame};
    for (unsigned i = 0; i < 6; ++i) {
        if (!methods[i] || !(wood.addresses[i] = patchlib_method_get_pointer(methods[i]))) {
            note("Wood combo disabled: hook signature or pointer mismatch."); return;
        }
    }
    for (unsigned i = 0; i < 6; ++i) {
        if (DobbyHook(wood.addresses[i], replacements[i], originals[i]) != 0) {
            wood_cleanup(); note("Wood combo disabled: native hook failed; rolled back."); return;
        }
        ++wood.hook_count;
    }
    wood.ready = true;
    wood.observed_animation = -1;
    wood_draw_init(main_type, player, entity);
    note("Wood combo ready: item 24, 3 stages, 800ms idle reset, one successful hit per NPC per swing. Charge deferred.");
}
