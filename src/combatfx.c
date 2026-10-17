#define _POSIX_C_SOURCE 200809L
#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>
#include <time.h>
#include <math.h>
#define TEF_API_IMPL 1
#include "patchlib/method.h"
#include "patchlib/field.h"
#include "mod_core.h"
#include "mod_logger.h"
#include "combo_math.h"
#include "combatfx_internal.h"
#include "global_fx.h"
#include "patchlib/struct/array.h"
#include "terraria/texture2d.h"
#include "vfx_image.h"

// API slots are populated by TEFKernel's shared plugin registration.
void (*mod_logger_write)(mod_log_level_t, const char *, const char *, ...) = NULL;
// Shared ABI types are declared in combatfx_internal.h.
GetVec get_screen;
GetCenter get_center;
SetVec set_screen;
GetInt get_player, get_width, get_height;
GetBool get_menu;
NewDust new_dust;
patch_handle_t m_center, m_screen, m_set_screen, m_player, m_width, m_height, m_menu, m_dust;
patch_handle_t f_id, f_friendly, f_netmode, f_dust_count, f_life;
static patch_hook_id_t hit_hook = PATCH_HOOK_INVALID_ID, camera_hook = PATCH_HOOK_INVALID_ID;
static bool enabled, busy, first_hit, offset_applied;
static Vec2 camera_base, camera_written;
static uint64_t shake_until, shake_start;
static float shake_strength;
static FILE *runtime_log;
char mod_resource_dir[1024];

uint64_t now_ms(void) {
    struct timespec t;
    if (clock_gettime(CLOCK_MONOTONIC, &t) != 0) return 0;
    return (uint64_t)t.tv_sec * 1000 + (uint64_t)t.tv_nsec / 1000000;
}
void note(const char *message) {
    if (mod_logger_write) mod_logger_write(MOD_LOG_LEVEL_INFO, "CombatFX", "%s", message);
    if (runtime_log) { fprintf(runtime_log, "%s\n", message); fflush(runtime_log); }
}
bool match(patch_handle_t method, bool instance, patch_type_t result,
                  const patch_type_t *types, size_t count) {
    if (!method) return false;
    patch_method_signature_t s = {0};
    if (!patchlib_method_get_signature(method, &s)) return false;
    bool ok = s.is_instance == instance && s.return_type == result && tefstd_vector_size(&s.arg_types) == count;
    for (size_t i = 0; ok && i < count; ++i) {
        const patch_type_t *actual = tefstd_vector_at(&s.arg_types, i);
        ok = actual && *actual == types[i];
    }
    patchlib_method_signature_free(&s);
    return ok;
}
patch_handle_t method(patch_handle_t type, const char *name, bool instance,
                              patch_type_t result, const patch_type_t *args, size_t count) {
    if (!type) return NULL;
    patch_handle_t m = patchlib_type_get_method_by_param_count(type, name, (int)count);
    return match(m, instance, result, args, count) ? m : NULL;
}
patch_handle_t field(patch_handle_t type, const char *name, size_t size, bool is_static) {
    if (!type) return NULL;
    patch_handle_t f = patchlib_type_get_field(type, name);
    if (!f || patchlib_field_get_size(f) != size || patchlib_field_is_static(f) != is_static) return NULL;
    return f;
}
bool playing(void) {
    if (get_menu(m_menu)) return false;
    int mode = 2;
    patchlib_field_get_value(f_netmode, NULL, &mode);
    return mode != 2;
}

static void restore_camera(void) {
    if (!offset_applied || !get_screen || !set_screen) return;
    Vec2 current = get_screen(m_screen);
    // Preserve any camera changes performed by other mods after our callback.
    if (fabsf(current.x - camera_written.x) < 0.01f && fabsf(current.y - camera_written.y) < 0.01f)
        set_screen(camera_base, m_set_screen);
    offset_applied = false;
}
static bool camera_prefix(patch_handle_t instance, void **args, const patch_method_signature_t *sig, void *result) {
    (void)instance; (void)args; (void)sig; (void)result;
    restore_camera();
    return false;
}
static void camera_postfix(patch_handle_t instance, void **args, void *result, const patch_method_signature_t *sig) {
    (void)instance; (void)args; (void)sig; (void)result;
    if (!enabled) return;
    if (!playing()) { shake_until = 0; fx_global_reset(); return; }
    uint64_t now = now_ms();
    fx_global_tick(now);
    if (now >= shake_until || now < shake_start) return;
    float remaining = (float)(shake_until - now) / 100.0f;
    float phase = (float)(now - shake_start);
    camera_base = get_screen(m_screen);
    camera_written = camera_base;
    camera_written.x += sinf(phase * 0.18f) * shake_strength * remaining;
    camera_written.y += cosf(phase * 0.23f) * shake_strength * remaining * 0.55f;
    set_screen(camera_written, m_set_screen);
    offset_applied = true;
}
static bool global_particle_sink(const FxParticle *particle) {
    if(!enabled || !new_dust || !particle || !isfinite(particle->position.x) || !isfinite(particle->position.y) ||
       !isfinite(particle->velocity_x) || !isfinite(particle->velocity_y) ||
       !isfinite(particle->scale) || particle->scale<0.1f || particle->scale>2.0f) return false;
    int active=0;
    if(f_dust_count) patchlib_field_get_value(f_dust_count,NULL,&active);
    if(active>=560) return false;
    Color color={0,0,0,0};
    Vec2 pos={particle->position.x,particle->position.y};
    return new_dust(pos,2,2,particle->dust_type,particle->velocity_x,particle->velocity_y,
        particle->alpha,color,particle->scale,m_dust)>=0;
}
static void hit_postfix(patch_handle_t npc, void **args, void *result, const patch_method_signature_t *sig) {
    (void)sig;
    weapon_modules_on_hit(npc,result);
    if(!enabled || busy || !npc || !args || !result || *(int *)result<=0) return;
    if(!args[0] || !args[3] || !args[4] || !args[5]) return;
    if(*(int *)args[0]<=0 || *(bool *)args[4] || !playing()) return;
    int local=get_player(m_player);
    if(local<0 || local>=255 || *(int *)args[5]!=local) return;
    bool friendly=false;
    patchlib_field_get_value(f_friendly,npc,&friendly);
    if(friendly) return;
    int id=-1; patchlib_field_get_value(f_id,npc,&id);
    Vec2 center=get_center(npc,m_center);
    if(!isfinite(center.x) || !isfinite(center.y)) return;
    Vec2 screen=get_screen(m_screen);
    int sw=get_width(m_width),sh=get_height(m_height);
    if(sw<=0 || sh<=0 || center.x<screen.x-32 || center.x>screen.x+sw+32 ||
       center.y<screen.y-32 || center.y>screen.y+sh+32) return;
    int life=1; bool lethal=false;
    if(f_life) { patchlib_field_get_value(f_life,npc,&life); lethal=life<=0; }
    FxNpcHit event={id,*(int *)result,{center.x,center.y},*(bool *)args[3],lethal,now_ms()};
    busy=true;
    bool emitted=fx_global_npc_hit(&event);
    if(emitted && camera_hook!=PATCH_HOOK_INVALID_ID && event.at_ms>=shake_until) {
        shake_start=event.at_ms; shake_until=event.at_ms+100;
        shake_strength=event.critical?2.0f:1.1f;
    }
    busy=false;
    if(emitted && !first_hit) {first_hit=true;note("First global impact event accepted; impact/death effects active.");}
}
static bool api_ready(void) {
    return patchlib_type_get_type && patchlib_type_get_field && patchlib_type_get_method_by_param_count &&
        patchlib_method_get_signature && patchlib_method_signature_free && patchlib_method_get_pointer &&
        patchlib_field_get_size && patchlib_field_is_static && patchlib_field_get_value &&
        patchlib_install_prepost_hook && patchlib_uninstall_hook && tefstd_vector_size && tefstd_vector_at;
}
static void init_mod(kernel_mod_handle_t *handle) {
    if (enabled) return;
    if (handle && handle->private_dir) {
        int copied = snprintf(mod_resource_dir,sizeof(mod_resource_dir),"%s",handle->private_dir);
        if (copied < 0 || (size_t)copied >= sizeof(mod_resource_dir)) mod_resource_dir[0] = 0;
        char path[1024]; int n = snprintf(path, sizeof(path), "%s/combatfx_runtime.log", handle->private_dir);
        if (n > 0 && (size_t)n < sizeof(path)) runtime_log = fopen(path, "w");
    }
    note("CombatFX 0.4.4 Android test build; global impact, death and biome ambience modules enabled.");
    if (!api_ready()) { note("Disabled: required kernel API unavailable."); return; }
    patch_handle_t main_type = patchlib_type_get_type("Terraria", "Main");
    patch_handle_t npc_type = patchlib_type_get_type("Terraria", "NPC");
    patch_handle_t entity = patchlib_type_get_type("Terraria", "Entity");
    patch_handle_t dust = patchlib_type_get_type("Terraria", "Dust");
    f_id = field(entity, "whoAmI", sizeof(int), false);
    f_friendly = field(npc_type, "friendly", sizeof(bool), false);
    f_netmode = field(main_type, "netMode", sizeof(int), true);
    f_dust_count = field(dust, "activeDustCount", sizeof(int), true);
    f_life = field(npc_type, "life", sizeof(int), false);
    m_center = method(entity, "get_Center", true, PATCH_STRUCT, NULL, 0);
    m_screen = method(main_type, "get_screenPosition", false, PATCH_STRUCT, NULL, 0);
    m_player = method(main_type, "get_myPlayer", false, PATCH_INT32, NULL, 0);
    m_menu = method(main_type, "get_gameMenu", false, PATCH_BOOL, NULL, 0);
    m_width = method(main_type, "get_screenWidth", false, PATCH_INT32, NULL, 0);
    m_height = method(main_type, "get_screenHeight", false, PATCH_INT32, NULL, 0);
    const patch_type_t dust_args[] = {PATCH_STRUCT, PATCH_INT32, PATCH_INT32, PATCH_INT32, PATCH_FLOAT,
        PATCH_FLOAT, PATCH_INT32, PATCH_STRUCT, PATCH_FLOAT};
    m_dust = method(dust, "NewDust", false, PATCH_INT32, dust_args, 9);
    const patch_type_t hit_args[] = {PATCH_INT32, PATCH_FLOAT, PATCH_INT32, PATCH_BOOL, PATCH_BOOL, PATCH_INT32};
    patch_handle_t strike = method(npc_type, "StrikeNPC", true, PATCH_INT32, hit_args, 6);
    if (!m_center || !f_id || !f_friendly || !f_netmode ||
        !m_screen || !m_player || !m_menu || !m_width || !m_height || !m_dust || !strike) {
        note("Disabled: game method signature or field layout mismatch."); return;
    }
    get_center = (GetCenter)patchlib_method_get_pointer(m_center);
    get_screen = (GetVec)patchlib_method_get_pointer(m_screen);
    get_player = (GetInt)patchlib_method_get_pointer(m_player);
    get_menu = (GetBool)patchlib_method_get_pointer(m_menu);
    get_width = (GetInt)patchlib_method_get_pointer(m_width);
    get_height = (GetInt)patchlib_method_get_pointer(m_height);
    new_dust = (NewDust)patchlib_method_get_pointer(m_dust);
    if (!get_center || !get_screen || !get_player || !get_menu || !get_width || !get_height || !new_dust) {
        note("Disabled: native method pointer unavailable."); return;
    }
    // Optional camera hook: particle feedback remains usable if camera APIs differ.
    const patch_type_t vec_arg[] = {PATCH_STRUCT};
    m_set_screen = method(main_type, "set_screenPosition", false, PATCH_VOID, vec_arg, 1);
    patch_handle_t camera = method(main_type, "DoDraw_UpdateCameraPosition", false, PATCH_VOID, NULL, 0);
    if (m_set_screen && camera) {
        set_screen = (SetVec)patchlib_method_get_pointer(m_set_screen);
        if (set_screen) camera_hook = patchlib_install_prepost_hook(camera, camera_prefix, camera_postfix);
    }
    fx_global_bind(global_particle_sink);
    hit_hook = patchlib_install_prepost_hook(strike, NULL, hit_postfix);
    if (hit_hook == PATCH_HOOK_INVALID_ID) {
        if (camera_hook != PATCH_HOOK_INVALID_ID) patchlib_uninstall_hook(camera_hook);
        camera_hook = PATCH_HOOK_INVALID_ID;
        note("Disabled: could not install hit hook."); return;
    }
    enabled = true;
    fx_global_init(main_type,patchlib_type_get_type("Terraria","Player"));
    weapon_modules_init(main_type, entity);
    note(camera_hook == PATCH_HOOK_INVALID_ID ? "Ready: particles only; camera API unavailable." :
         "Ready: global hit/death effects, capped biome ambience + camera feedback.");
}
static void cleanup_mod(kernel_mod_handle_t *handle) {
    (void)handle;
    weapon_modules_cleanup();
    fx_global_shutdown();
    enabled = false;
    if (patchlib_uninstall_hook) {
        if (hit_hook != PATCH_HOOK_INVALID_ID) patchlib_uninstall_hook(hit_hook);
        if (camera_hook != PATCH_HOOK_INVALID_ID) patchlib_uninstall_hook(camera_hook);
    }
    hit_hook = camera_hook = PATCH_HOOK_INVALID_ID;
    // Do not invoke game methods while a mod is being unloaded on another thread.
    offset_applied = false; shake_until = 0; busy = false; first_hit = false;
    note("CombatFX unloaded.");
    if (runtime_log) { fclose(runtime_log); runtime_log = NULL; }
}
static kernel_mod_info_t *get_info(void) {
    static kernel_mod_info_t info = {"liuxin.combatfx", 5, 1, "0.4.4"}; return &info;
}
kernel_mod_ops_t *create_kernel_mod(void) {
    static kernel_mod_ops_t ops = {init_mod, cleanup_mod, get_info}; return &ops;
}
