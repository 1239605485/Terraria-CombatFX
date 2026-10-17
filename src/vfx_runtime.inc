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
    struct { FxGCHandle root; int width,height; bool logged; } sprites[9];
} slash;
static const char *slash_name="slash-tbbk.png";
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
    char path[1400];
    int n=snprintf(path,sizeof(path),"%s/textures/%s",mod_resource_dir,slash_name);
    FxImage image;
    if(n<=0 || (size_t)n>=sizeof(path) || !fx_image_load(path,256,&image)) { note("Slash sheet load failed."); return; }
    if(image.width!=192 || image.height!=141) { note("Slash sheet dimensions invalid."); fx_image_free(&image); return; }
    // Preserve the downloaded original; slice its 3x3 grid only during GPU upload.
    for(unsigned i=0;i<9;i++) {
        unsigned char pixels[64*47*4];
        for(unsigned y=0;y<47;y++) memcpy(pixels+y*64*4,image.pixels+((i/3*47+y)*192+i%3*64)*4,64*4);
        void *texture=terraria_texture2d_create(64,47,TEXTURE_FORMAT_RGBA32,pixels,sizeof(pixels));
        if(texture) slash.sprites[i].root=slash.alloc(texture,2,slash.ma);
        slash.sprites[i].width=64; slash.sprites[i].height=47;
    }
    fx_image_free(&image);
    note("CC0 tbbk slash sheet uploaded: 9 frames, 64x47 each.");
}

// Normal alpha blending needs premultiplied tint RGB, including the outer layer.
// Use the same ambient itemColor as the non-emissive wooden sword. Fade alpha
// with light as well, so an unlit trail neither glows nor stamps a black arc.
static Color wood_vfx_lit_color(Color light,float opacity,uint8_t r,uint8_t g,uint8_t b) {
    unsigned peak=light.r;
    if (light.g>peak) peak=light.g;
    if (light.b>peak) peak=light.b;
    float factor=fx_clamp(opacity,0,1)*light.a/255.0f;
    return (Color){(uint8_t)(peak*factor),
        (uint8_t)(light.b*factor*b/255.0f),
        (uint8_t)(light.g*factor*g/255.0f),
        (uint8_t)(light.r*factor*r/255.0f)};
}
static void wood_vfx_emit(void *drawinfo, void *player, Vec2 draw_offset) {
    (void)draw_offset; // PlayerDrawSet's screen-space offset is already represented by Main.screenPosition.
    if (!slash.ready || !wood.sample_active || !wood.combo.running) return;
    wood_vfx_load();
    unsigned stage = wood.combo.stage % 3;
    float p = fx_swing_fraction(stage,wood.visual_progress);
    unsigned frame=(unsigned)fx_clamp(p*9,0,8);
    if (!slash.sprites[frame].root.value) return;
    void *texture=slash.target(&slash.sprites[frame].root,slash.mt);
    if(!texture) return;
    float opacity = sinf(p*FX_PI)*0.62f;
    if (opacity < 0.03f) return;
    int direction = 1; patchlib_field_get_value(wood.direction,player,&direction);
    float item_scale = wood.scale(player,wood.held(player,wood.mh),wood.mscale);
    if (!isfinite(item_scale) || item_scale <= 0 || item_scale > 4) return;
    Vec2 screen=get_screen(m_screen);
    FxPose pose=fx_pose(stage,wood.visual_progress);
    float blade_angle=fx_world_angle(pose.angle,direction);
    float slash_angle=blade_angle+FX_PI*0.5f;
    float length=wood.length*item_scale*wood.visual_stretch;
    // visual_hand and blade reach are world coordinates. Convert to screen space exactly once;
    // adding draw_offset here applied the camera displacement a second time and sent the slash away.
    Vec2 position={wood.visual_hand.x+cosf(blade_angle)*length*0.48f-screen.x,
                   wood.visual_hand.y+sinf(blade_angle)*length*0.48f-screen.y};
    Vec2 origin={32,23.5f};
    Vec2 scale={length*0.92f/64,length*(stage==0 ? 0.34f : 0.55f)/47};
    Color light={0};
    patchlib_field_get_value(wood_view.item_color,drawinfo,&light);
    Color tint=wood_vfx_lit_color(light,opacity,255,255,255);
    if (tint.a<3) return;
    int flip=((stage==2) != (direction<0)) ? 2 : 0;
    // Follow the layered sword trail used by tModLoader's Excalibur example:
    // broad low-opacity color, the full slash, then a narrow bright core.
    // Keep this to three cached sprites per active swing for mobile performance.
    Color outer=wood_vfx_lit_color(light,opacity*0.34f,255,128,48);
    Color core=wood_vfx_lit_color(light,opacity*0.52f,255,255,255);
    Vec2 outer_scale={scale.x*1.10f,scale.y*1.14f};
    Vec2 core_scale={scale.x*0.72f,scale.y*0.58f};
    void *entry=wood_view.add(drawinfo,wood_view.madd);
    if (!entry) return;
    wood_view.set(entry,texture,position,outer,slash_angle,origin,outer_scale,flip,0,wood_view.mset);
    entry=wood_view.add(drawinfo,wood_view.madd);
    if (!entry) return;
    wood_view.set(entry,texture,position,tint,slash_angle,origin,scale,flip,0,wood_view.mset);
    entry=wood_view.add(drawinfo,wood_view.madd);
    if (!entry) return;
    wood_view.set(entry,texture,position,core,slash_angle,origin,core_scale,flip,0,wood_view.mset);
    if (!slash.sprites[stage].logged) {
        slash.sprites[stage].logged = true;
        char line[140]; snprintf(line,sizeof(line),"Slash sprite emitted: stage=%u %s",stage+1,slash_name); note(line);
    }
}
static void wood_vfx_cleanup(void) {
    if (slash.release) for (unsigned i = 0; i < 9; ++i)
        if (slash.sprites[i].root.value) slash.release(&slash.sprites[i].root,slash.mf);
    memset(&slash,0,sizeof(slash));
}
