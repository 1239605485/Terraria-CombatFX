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
#include "vfx_runtime.inc"
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
