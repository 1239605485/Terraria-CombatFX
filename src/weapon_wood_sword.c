// Included after the base helpers; no retained game objects.
#include "dobby.h"
#include "combatfx_internal.h"
#include <math.h>
#include <stdio.h>
#include <string.h>
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
    float local_angle = fx_swing_angle(wood.combo.stage, progress);
    float pose_angle = fx_project_angle(wood.combo.stage, local_angle);
    if (wood.combo.stage == 0 && progress < 0.26f)
        pose_angle = 2.55f + (pose_angle - 2.55f) * fx_draw_lift(progress);
    float world_angle = fx_world_angle(pose_angle, direction);
    float arm_angle = world_angle - FX_PI * 0.5f;
    // Full is enum value 0 in this mobile version.
    wood.arm(player, true, 0, arm_angle, wood.ma);
    Vec2 hand = wood.hand(player, 0, arm_angle, wood.mhand);
    if (!isfinite(hand.x) || !isfinite(hand.y) || fabsf(hand.x) > 1e8f || fabsf(hand.y) > 1e8f) return false;
    if (wood.combo.stage == 0 && progress < 0.26f) {
        Vec2 center = get_center(player, m_center);
        float lift = fx_draw_lift(progress);
        Vec2 hip = {center.x + direction * 5, center.y + 5};
        hand.x = hip.x + (hand.x - hip.x) * lift;
        hand.y = hip.y + (hand.y - hip.y) * lift;
    }
    if (wood.combo.stage == 0) hand.x += direction*8*fx_swing_fraction(0,progress);
    float rotation = direction * (pose_angle + FX_PI * 0.25f);
    wood.visual_hand = hand; wood.visual_rotation = rotation; wood.visual_progress = progress;
    wood.arm_rotation = arm_angle;
    wood.visual_stretch = fx_project_length(wood.combo.stage,local_angle);
    if (wood.combo.stage == 0 && progress < 0.26f)
        wood.visual_stretch = 0.55f + (wood.visual_stretch - 0.55f) * fx_draw_lift(progress);
    void *location = patchlib_field_get_pointer(wood.location, player);
    if (!location) return false;
    // The supplied kernel reports sizeof(void*) for value-type fields on arm32.
    memcpy(location, &hand, sizeof(hand));
    patchlib_field_set_value(wood.rotation, player, &rotation);
    wood.padding = 3.0f * scale;
    float previous = fmaxf(0.16f, progress - 1.0f / (float)(maximum - 1));
    for (unsigned i = 0; i < FX_SWEEP_SAMPLES; ++i) {
        float t = previous + (progress - previous) * (float)i / (FX_SWEEP_SAMPLES - 1);
        float a = fx_swing_angle(wood.combo.stage, t);
        float angle = fx_world_angle(fx_project_angle(wood.combo.stage,a), direction);
        float reach = wood.length * scale * fx_project_length(wood.combo.stage,a);
        Vec2 h = wood.hand(player, 0, angle - FX_PI * 0.5f, wood.mhand);
        if (!isfinite(h.x) || !isfinite(h.y) || fabsf(h.x) > 1e8f || fabsf(h.y) > 1e8f) return false;
        if (wood.combo.stage == 0) h.x += direction*8*fx_swing_fraction(0,t);
        wood.blades[i].hand = h;
        wood.blades[i].tip = (Vec2){h.x + cosf(angle) * reach, h.y + sinf(angle) * reach};
    }
    wood.sample_active = fx_swing_active(progress) && (wood.combo.stage != 0 || progress >= 0.26f);
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
void wood_record_hit(patch_handle_t npc, void *result) {
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
void wood_cleanup(void) {
    wood_draw_cleanup();
    wood.ready = false;
    for (unsigned i = wood.hook_count; i > 0; --i) DobbyDestroy(wood.addresses[i-1]);
    memset(&wood, 0, sizeof(wood)); fx_combo_reset(&wood.combo);
    wood_process_npc = -1; wood_process_depth = 0;
}
void wood_init(patch_handle_t main_type, patch_handle_t entity) {
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

// Uses the game's held-item layer and vanilla wooden-sword texture. No new assets.
typedef struct {
    void *texture;
    Vec2 position;
    FxRect source;
    Color color;
    float rotation;
    Vec2 origin, scale;
    int effect, shader;
    bool ignore_player_rotation;
} WoodDrawData;
_Static_assert(sizeof(WoodDrawData) == (sizeof(void *) == 8 ? 72 : 64), "DrawData layout");
typedef void (*WoodDrawOriginal)(void *, const void *);
typedef void *(*WoodAddDraw)(void *, const void *);
typedef void (*WoodSetDraw)(void *, void *, Vec2, Color, float, Vec2, Vec2, int, int, const void *);
static struct {
    bool ready, logged, busy;
    void *address;
    WoodDrawOriginal original;
    WoodAddDraw add;
    WoodSetDraw set;
    patch_handle_t madd, mset;
    patch_handle_t player, position, location, arm_rotation, shadow, cache, count, item_scale, player_width, player_height;
} wood_view;
// Strong managed handles keep uploaded textures alive across frames and weapon switches.
typedef struct { uintptr_t value; } FxGCHandle;
typedef FxGCHandle (*FxGCAlloc)(void *, int, const void *);
typedef void *(*FxGCTarget)(FxGCHandle *, const void *);
typedef void (*FxGCFree)(FxGCHandle *, const void *);
static struct {
    bool ready, tried;
    FxGCAlloc alloc;
    FxGCTarget target;
    FxGCFree release;
    patch_handle_t ma, mt, mf;
    struct { FxGCHandle root; int width,height; bool logged; } sprites[3];
} slash;
static const char *slash_names[] = {"slash-forward.png","slash-down.png","slash-up.png"};
static void wood_vfx_init(void) {
    if (!terraria_texture2d_create || !mod_resource_dir[0]) { note("Slash textures disabled: texture API or resource directory missing."); return; }
    patch_handle_t gc = patchlib_type_get_type("System.Runtime.InteropServices","GCHandle");
    const patch_type_t args[] = {PATCH_OBJECT,PATCH_INT32};
    slash.ma = method(gc,"Alloc",false,PATCH_STRUCT,args,2);
    slash.mt = method(gc,"get_Target",true,PATCH_OBJECT,NULL,0);
    slash.mf = method(gc,"Free",true,PATCH_VOID,NULL,0);
    if (!slash.ma || !slash.mt || !slash.mf) { note("Slash textures disabled: GCHandle signatures unavailable."); return; }
    slash.alloc = (FxGCAlloc)patchlib_method_get_pointer(slash.ma);
    slash.target = (FxGCTarget)patchlib_method_get_pointer(slash.mt);
    slash.release = (FxGCFree)patchlib_method_get_pointer(slash.mf);
    slash.ready = slash.alloc && slash.target && slash.release;
    if (slash.ready) note("Slash textures ready: PNG resources will load once in the held-item draw callback.");
}
static void wood_vfx_load(void) {
    if (!slash.ready || slash.tried) return;
    slash.tried = true;
    for (unsigned i = 0; i < 3; ++i) {
        char path[1400];
        int n = snprintf(path,sizeof(path),"%s/textures/%s",mod_resource_dir,slash_names[i]);
        FxImage image;
        if (n <= 0 || (size_t)n >= sizeof(path) || !fx_image_load(path,256,&image)) {
            char line[160]; snprintf(line,sizeof(line),"Slash PNG load failed: %s",slash_names[i]); note(line); continue;
        }
        void *texture = terraria_texture2d_create(image.width,image.height,TEXTURE_FORMAT_RGBA32,image.pixels,image.size);
        if (texture) slash.sprites[i].root = slash.alloc(texture,2,slash.ma); // GCHandleType.Normal = 2
        if (texture && slash.sprites[i].root.value) {
            slash.sprites[i].width = image.width; slash.sprites[i].height = image.height;
            char line[180]; snprintf(line,sizeof(line),"Slash PNG loaded: %s -> %dx%d, %zu RGBA bytes",slash_names[i],image.width,image.height,image.size); note(line);
        } else note("Slash texture creation or rooting failed.");
        fx_image_free(&image);
    }
}
static void wood_vfx_emit(void *drawinfo, void *player, Vec2 draw_offset) {
    if (!slash.ready || !wood.sample_active || !wood.combo.running) return;
    wood_vfx_load();
    unsigned stage = wood.combo.stage % 3;
    if (!slash.sprites[stage].root.value) return;
    void *texture = slash.target(&slash.sprites[stage].root,slash.mt);
    if (!texture) return;
    float p = fx_swing_fraction(stage,wood.visual_progress);
    float opacity = sinf(p*FX_PI)*0.92f;
    if (opacity < 0.03f) return;
    int direction = 1; patchlib_field_get_value(wood.direction,player,&direction);
    float item_scale = wood.scale(player,wood.held(player,wood.mh),wood.mscale);
    if (!isfinite(item_scale) || item_scale <= 0 || item_scale > 4) return;
    Vec2 center = get_center(player,m_center), screen = get_screen(m_screen);
    float width = wood.length*item_scale*(stage == 0 ? 1.15f : 1.25f);
    float height = wood.length*item_scale*(stage == 0 ? 0.48f : 1.8f);
    Vec2 position = {center.x + direction*7 - screen.x + draw_offset.x, center.y - screen.y + draw_offset.y};
    if (stage == 1) position.y += (p-0.5f)*8;
    if (stage == 2) position.y -= (p-0.5f)*8;
    int w = slash.sprites[stage].width, h = slash.sprites[stage].height;
    Vec2 origin = {direction > 0 ? w*0.05f : w*0.95f,h*0.5f};
    Vec2 scale = {width/w,height/h};
    uint8_t value = (uint8_t)(255*opacity);
    Color tint = {value,value,value,value};
    void *entry = wood_view.add(drawinfo,wood_view.madd);
    if (!entry) return;
    wood_view.set(entry,texture,position,tint,0,origin,scale,direction > 0 ? 0 : 1,0,wood_view.mset);
    if (!slash.sprites[stage].logged) {
        slash.sprites[stage].logged = true;
        char line[140]; snprintf(line,sizeof(line),"Slash sprite emitted: stage=%u %s",stage+1,slash_names[stage]); note(line);
    }
}
static void wood_vfx_cleanup(void) {
    if (slash.release) for (unsigned i = 0; i < 3; ++i)
        if (slash.sprites[i].root.value) slash.release(&slash.sprites[i].root,slash.mf);
    memset(&slash,0,sizeof(slash));
}

static bool wood_read_vec(patch_handle_t f, void *object, Vec2 *out) {
    void *p = patchlib_field_get_pointer(f, object);
    if (!p) return false;
    memcpy(out,p,sizeof(*out));
    return isfinite(out->x) && isfinite(out->y);
}
static bool wood_write_vec(patch_handle_t f, void *object, Vec2 value) {
    void *p = patchlib_field_get_pointer(f,object);
    if (!p) return false;
    memcpy(p,&value,sizeof(value)); return true;
}
static patch_handle_t wood_overload(patch_handle_t type, const char *name, const patch_type_t *args, size_t count) {
    tefstd_vector_t methods = {0};
    if (!patchlib_type_get_methods(type,false,&methods)) return NULL;
    patch_handle_t found = NULL;
    for (size_t i = 0; i < tefstd_vector_size(&methods); ++i) {
        patch_handle_t m = *(patch_handle_t *)tefstd_vector_at(&methods,i);
        const char *n = patchlib_method_get_name(m);
        if (n && strcmp(n,name) == 0 && match(m,true,PATCH_VOID,args,count)) { found = m; break; }
    }
    tefstd_vector_destroy(&methods);
    return found;
}
static void wood_native_draw(void *drawinfo, const void *info) {
    if (!wood_view.ready || wood_view.busy || !drawinfo) { wood_view.original(drawinfo,info); return; }
    void *player = NULL;
    patchlib_field_get_value(wood_view.player,drawinfo,&player);
    if (!wood_local(player)) { wood_view.original(drawinfo,info); return; }
    void *item = wood.held(player,wood.mh);
    if (!wood_can_animate(player,item)) { wood_view.original(drawinfo,info); return; }
    float shadow = 0; patchlib_field_get_value(wood_view.shadow,drawinfo,&shadow);
    if (shadow > 0) { wood_view.original(drawinfo,info); return; }
    wood_vfx_load();
    int animation = 0, direction = 0, before = 0;
    float rotation = 0, item_scale = 1, arm_rotation = 0;
    Vec2 old_location, old_draw_location, draw_position;
    patchlib_field_get_value(wood.animation,player,&animation);
    patchlib_field_get_value(wood.direction,player,&direction);
    patchlib_field_get_value(wood.rotation,player,&rotation);
    patchlib_field_get_value(wood_view.item_scale,item,&item_scale);
    patchlib_field_get_value(wood_view.arm_rotation,drawinfo,&arm_rotation);
    patchlib_field_get_value(wood_view.count,drawinfo,&before);
    if ((direction != 1 && direction != -1) || !isfinite(item_scale) || item_scale <= 0 ||
        !wood_read_vec(wood.location,player,&old_location) || !wood_read_vec(wood_view.location,drawinfo,&old_draw_location) ||
        !wood_read_vec(wood_view.position,drawinfo,&draw_position)) { wood_view.original(drawinfo,info); return; }
    bool swinging = animation > 0;
    if (swinging && !wood_pose(player,item)) { wood_view.original(drawinfo,info); return; }
    Vec2 center = get_center(player,m_center), hand;
    int pw = 0, ph = 0;
    patchlib_field_get_value(wood_view.player_width,player,&pw);
    patchlib_field_get_value(wood_view.player_height,player,&ph);
    Vec2 draw_offset = {draw_position.x - (center.x - pw * 0.5f), draw_position.y - (center.y - ph * 0.5f)};
    float chosen_rotation, stretch;
    if (swinging) { hand = wood.visual_hand; chosen_rotation = wood.visual_rotation; stretch = wood.visual_stretch; }
    else {
        // Hilt at the belt; short blade points diagonally backwards.
        hand = (Vec2){center.x + direction * 5,center.y + 5};
        chosen_rotation = direction * (2.55f + FX_PI * 0.25f); stretch = 0.55f;
    }
    Vec2 snapshot_hand = {hand.x + draw_offset.x,hand.y + draw_offset.y};
    float drawn_scale = item_scale * stretch;
    if (!isfinite(snapshot_hand.x) || !isfinite(snapshot_hand.y) || !isfinite(drawn_scale)) { wood_view.original(drawinfo,info); return; }
    wood_view.busy = true;
    // Temporary draw-only values are restored before leaving this layer.
    int draw_animation = animation > 0 ? animation : 1;
    patchlib_field_set_value(wood.animation,player,&draw_animation);
    patchlib_field_set_value(wood.rotation,player,&chosen_rotation);
    patchlib_field_set_value(wood_view.item_scale,item,&drawn_scale);
    wood_write_vec(wood.location,player,hand);
    wood_write_vec(wood_view.location,drawinfo,snapshot_hand);
    if (swinging) patchlib_field_set_value(wood_view.arm_rotation,drawinfo,&wood.arm_rotation);
    wood_view.original(drawinfo,info);
    patchlib_field_set_value(wood.animation,player,&animation);
    patchlib_field_set_value(wood.rotation,player,&rotation);
    patchlib_field_set_value(wood_view.item_scale,item,&item_scale);
    wood_write_vec(wood.location,player,old_location);
    wood_write_vec(wood_view.location,drawinfo,old_draw_location);
    patchlib_field_set_value(wood_view.arm_rotation,drawinfo,&arm_rotation);
    wood_view.busy = false;
    int after = 0;
    patchlib_field_get_value(wood_view.count,drawinfo,&after);
    if (after <= before) return;
    if (!wood_view.logged) { wood_view.logged = true; note("Wood held-item draw layer emitted sword; waist/draw override active."); }
    if (swinging) wood_vfx_emit(drawinfo,player,draw_offset);
}
static void wood_draw_cleanup(void) {
    wood_view.ready = false;
    wood_vfx_cleanup();
    if (wood_view.address) DobbyDestroy(wood_view.address);
    memset(&wood_view,0,sizeof(wood_view));
}
static void wood_draw_init(patch_handle_t main_type, patch_handle_t player, patch_handle_t entity) {
    (void)main_type; (void)player;
    if (!patchlib_type_get_methods || !patchlib_method_get_name || !tefstd_vector_destroy) { note("Wood draw override unavailable: enumeration API missing."); return; }
    patch_handle_t set = patchlib_type_get_type("Terraria.DataStructures","PlayerDrawSet");
    patch_handle_t layers = patchlib_type_get_type("Terraria.DataStructures","PlayerDrawLayers");
    patch_handle_t data = patchlib_type_get_type("Terraria.DataStructures","DrawData");
    patch_handle_t item = patchlib_type_get_type("Terraria","Item");
    if (!set || !layers || !data || !item) { note("Wood draw override unavailable: draw types missing."); return; }
    wood_view.player = field(set,"drawPlayer",sizeof(void *),false);
    wood_view.position = patchlib_type_get_field(set,"Position");
    wood_view.location = patchlib_type_get_field(set,"ItemLocation");
    wood_view.arm_rotation = field(set,"compositeFrontArmRotation",sizeof(float),false);
    wood_view.shadow = field(set,"shadow",sizeof(float),false);
    wood_view.cache = field(set,"DrawDataCache",sizeof(void *),false);
    wood_view.count = field(set,"DrawDataCacheCount",sizeof(int),false);
    wood_view.item_scale = field(item,"scale",sizeof(float),false);
    wood_view.player_width = field(entity,"width",sizeof(int),false);
    wood_view.player_height = field(entity,"height",sizeof(int),false);
    const patch_type_t draw_arg[] = {PATCH_OBJECT};
    const patch_type_t set_args[] = {PATCH_OBJECT,PATCH_STRUCT,PATCH_STRUCT,PATCH_FLOAT,PATCH_STRUCT,PATCH_STRUCT,PATCH_INT32,PATCH_INT32};
    patch_handle_t draw = method(layers,"DrawPlayer_27_HeldItem",false,PATCH_VOID,draw_arg,1);
    wood_view.madd = method(set,"AddDrawData",true,PATCH_POINTER,NULL,0);
    wood_view.mset = wood_overload(data,"Set",set_args,8);
    if (!draw || !wood_view.madd || !wood_view.mset || !wood_view.player || !wood_view.position || !wood_view.location ||
        !wood_view.arm_rotation || !wood_view.shadow || !wood_view.cache || !wood_view.count || !wood_view.item_scale ||
        !wood_view.player_width || !wood_view.player_height) { note("Wood draw override unavailable: field/method mismatch."); return; }
    wood_view.add = (WoodAddDraw)patchlib_method_get_pointer(wood_view.madd);
    wood_view.set = (WoodSetDraw)patchlib_method_get_pointer(wood_view.mset);
    void *address = patchlib_method_get_pointer(draw);
    if (!wood_view.add || !wood_view.set || !address || DobbyHook(address,(void *)wood_native_draw,(void **)&wood_view.original) != 0) {
        note("Wood draw override unavailable: native hook failed."); return;
    }
    wood_view.address = address; wood_view.ready = true;
    wood_vfx_init();
    note("Wood draw override ready: waist carry, first-hit unsheathing, forward/down/up moves with dedicated PNG slash textures.");
}
