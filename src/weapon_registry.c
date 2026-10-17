#include "combatfx_internal.h"

// Registration is the only shared edit point when adding a weapon source file.
// Each weapon keeps its hooks, animation data and draw state inside its own module.
typedef struct {
    const char *id;
    void (*initialize)(patch_handle_t main_type, patch_handle_t entity);
    void (*cleanup)(void);
    void (*on_hit)(patch_handle_t npc, void *result);
} WeaponModule;

extern void wood_init(patch_handle_t main_type, patch_handle_t entity);
extern void wood_cleanup(void);
extern void wood_record_hit(patch_handle_t npc, void *result);

static const WeaponModule modules[] = {
    {"wooden_sword", wood_init, wood_cleanup, wood_record_hit},
};

void weapon_modules_init(patch_handle_t main_type, patch_handle_t entity) {
    for (size_t i = 0; i < sizeof(modules) / sizeof(modules[0]); ++i)
        modules[i].initialize(main_type, entity);
}

void weapon_modules_cleanup(void) {
    for (size_t i = sizeof(modules) / sizeof(modules[0]); i > 0; --i)
        modules[i - 1].cleanup();
}

void weapon_modules_on_hit(patch_handle_t npc, void *result) {
    for (size_t i = 0; i < sizeof(modules) / sizeof(modules[0]); ++i)
        modules[i].on_hit(npc, result);
}
