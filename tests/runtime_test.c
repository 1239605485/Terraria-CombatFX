#define _POSIX_C_SOURCE 200809L
#include <assert.h>
#include "../src/combatfx.c"
static int spawned;
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
    (void)x; (void)y; (void)alpha; (void)color; (void)scale; (void)m;
    assert(isfinite(pos.x) && isfinite(pos.y)); assert(w == 6 && h == 6 && type == 228);
    return spawned++;
}
static void read_field(patch_handle_t f, patch_handle_t obj, void *out) {
    (void)obj;
    if (f == f_position) *(Vec2 *)out = (Vec2){100, 100};
    else if (f == f_width || f == f_height) *(int *)out = 20;
    else if (f == f_id) *(int *)out = 1;
    else if (f == f_friendly) *(bool *)out = false;
    else if (f == f_netmode) *(int *)out = mode;
}
int main(void) {
    // Missing injected APIs must disable the mod without dereferencing null slots.
    init_mod(NULL); assert(!enabled); cleanup_mod(NULL);
    f_position = (void *)1; f_width = (void *)2; f_height = (void *)3;
    f_id = (void *)4; f_friendly = (void *)5; f_netmode = (void *)6;
    patchlib_field_get_value = read_field;
    get_player = local_player; get_width = width; get_height = height;
    get_menu = in_menu; get_screen = screen_get; set_screen = screen_set;
    new_dust = dust_spawn; enabled = true;
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
    hit_postfix((void *)1, args, &result, NULL); assert(spawned == 3);
    hit_postfix((void *)1, args, &result, NULL); assert(spawned == 3);
    fx_budget_reset(&budget); crit = true;
    hit_postfix((void *)1, args, &result, NULL); assert(spawned == 8);
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
