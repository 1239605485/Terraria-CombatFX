#ifndef COMBATFX_INTERNAL_H
#define COMBATFX_INTERNAL_H
#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>
#include "patchlib/method.h"
#include "patchlib/field.h"
#include "patchlib/struct/array.h"
#include "terraria/texture2d.h"
#include "vfx_image.h"
#include "combo_math.h"
typedef FxPoint Vec2;
typedef struct { uint8_t a, b, g, r; } Color;
_Static_assert(sizeof(Vec2) == 8, "Vector2 ABI");
_Static_assert(sizeof(Color) == 4, "Color ABI");
typedef Vec2 (*GetVec)(const void *);
typedef Vec2 (*GetCenter)(void *, const void *);
typedef void (*SetVec)(Vec2, const void *);
typedef int (*GetInt)(const void *);
typedef bool (*GetBool)(const void *);
typedef int (*NewDust)(Vec2, int, int, int, float, float, int, Color, float, const void *);
extern GetVec get_screen;
extern GetCenter get_center;
extern SetVec set_screen;
extern GetInt get_player, get_width, get_height;
extern GetBool get_menu;
extern NewDust new_dust;
extern patch_handle_t m_center, m_screen, m_set_screen, m_player, m_width, m_height, m_menu, m_dust;
extern patch_handle_t f_id, f_friendly, f_netmode, f_dust_count;
extern char mod_resource_dir[1024];
uint64_t now_ms(void);
void note(const char *message);
bool match(patch_handle_t method, bool instance, patch_type_t result, const patch_type_t *types, size_t count);
patch_handle_t method(patch_handle_t type, const char *name, bool instance, patch_type_t result, const patch_type_t *args, size_t count);
patch_handle_t field(patch_handle_t type, const char *name, size_t size, bool is_static);
bool playing(void);
void wood_init(patch_handle_t main_type, patch_handle_t entity);
void wood_cleanup(void);
void wood_record_hit(patch_handle_t npc, void *result);
#endif
