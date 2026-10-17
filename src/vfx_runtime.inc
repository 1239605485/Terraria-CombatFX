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
