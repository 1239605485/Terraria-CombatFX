#define _POSIX_C_SOURCE 200809L
#include <assert.h>
#include "../src/combatfx.c"
static int spawned;
static int npc_life=100;
static Vec2 center_get(void *npc, const void *m) { (void)npc; (void)m; return (Vec2){110, 110}; }
static bool menu;
static int mode;
static Vec2 screen_value;
static int local_player(const void *m) { (void)m; return 0; }
static int width(const void *m) { (void)m; return 800; }
static int height(const void *m) { (void)m; return 600; }
static bool in_menu(const void *m) { (void)m; return menu; }
static Vec2 screen_get(const void *m) { (void)m; return screen_value; }
static void screen_set(Vec2 value, const void *m) { (void)m; screen_value = value; }
static int dust_spawn(Vec2 pos, int w, int h, int type, float x, float y, int alpha, Color color, float scale, const void *m) {
    (void)x; (void)y; (void)alpha; (void)color; (void)m;
    assert(isfinite(pos.x) && isfinite(pos.y)); assert(w == 2 && h == 2);
    assert(type==228 || type==27); assert(scale>=0.1f && scale<=2.0f);
    return spawned++;
}
static void read_field(patch_handle_t f, patch_handle_t obj, void *out) {
    (void)obj;
    if (f == f_id) *(int *)out = 1;
    else if (f == f_friendly) *(bool *)out = false;
    else if (f == f_netmode) *(int *)out = mode;
    else if (f == f_life) *(int *)out = npc_life;
    else if (f == f_dust_count) *(int *)out = spawned;
}
int main(void) {
    // Missing injected APIs must disable the mod without dereferencing null slots.
    init_mod(NULL); assert(!enabled); cleanup_mod(NULL);
    f_id = (void *)4; f_friendly = (void *)5; f_netmode = (void *)6;
    f_life=(void *)7; f_dust_count=(void *)8;
    patchlib_field_get_value = read_field;
    get_center = center_get; get_player = local_player; get_width = width; get_height = height;
    get_menu = in_menu; get_screen = screen_get; set_screen = screen_set;
    new_dust = dust_spawn; fx_global_bind(global_particle_sink); fx_global_reset(); enabled = true;
    int damage = 10, direction = 1, owner = 1, result = 10;
    float kb = 1; bool crit = false, from_net = false;
    void *args[] = {&damage, &kb, &direction, &crit, &from_net, &owner};
    hit_postfix((void *)1, args, &result, NULL); assert(spawned == 0);
    owner = 0; from_net = true;
    hit_postfix((void *)1, args, &result, NULL); assert(spawned == 0);
    from_net = false; menu = true;
    hit_postfix((void *)1, args, &result, NULL); assert(spawned == 0);
    menu = false; mode = 2;
    hit_postfix((void *)1, args, &result, NULL); assert(spawned == 0);
    mode = 0; result = 0;
    hit_postfix((void *)1, args, &result, NULL); assert(spawned == 0);
    result = 10; screen_value = (Vec2){10000, 10000};
    hit_postfix((void *)1, args, &result, NULL); assert(spawned == 0);
    screen_value = (Vec2){0, 0};
    hit_postfix((void *)1, args, &result, NULL); assert(spawned == 4);
    hit_postfix((void *)1, args, &result, NULL); assert(spawned == 4);
    fx_global_reset(); crit = true;
    hit_postfix((void *)1, args, &result, NULL); assert(spawned == 10);
    fx_global_reset(); npc_life=0;
    hit_postfix((void *)1,args,&result,NULL); assert(spawned==22);
    shake_start = now_ms(); shake_until = shake_start + 100; shake_strength = 2;
    camera_postfix(NULL, NULL, NULL, NULL); assert(offset_applied);
    camera_prefix(NULL, NULL, NULL, NULL);
    assert(!offset_applied && screen_value.x == 0 && screen_value.y == 0);
    camera_postfix(NULL, NULL, NULL, NULL);
    screen_value = (Vec2){50, 50}; // Other mod changed the camera.
    camera_prefix(NULL, NULL, NULL, NULL);
    assert(screen_value.x == 50 && screen_value.y == 50);
    return 0;
}
