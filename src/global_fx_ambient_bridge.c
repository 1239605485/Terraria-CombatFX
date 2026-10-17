#include "combatfx_internal.h"
#include "global_fx_private.h"
#include <string.h>

typedef bool (*FxZoneGetter)(void *,const void *);
static struct {
    bool ready;
    patch_handle_t players;
    FxZoneGetter zones[7];
    uint64_t last_sample;
} ambient_bridge;

static bool fx_zone(void *player,unsigned index) {
    return ambient_bridge.zones[index] && ambient_bridge.zones[index](player,NULL);
}
void fx_global_ambient_init(void *main_type,void *player_type) {
    memset(&ambient_bridge,0,sizeof(ambient_bridge));
    ambient_bridge.players=field(main_type,"player",sizeof(void *),true);
    static const char *names[]={"get_ZoneSnow","get_ZoneHallow","get_ZoneJungle","get_ZoneCorrupt",
        "get_ZoneCrimson","get_ZoneDesert","get_ZoneUnderworldHeight"};
    for(unsigned i=0;i<7;i++) {
        patch_handle_t m=method(player_type,names[i],true,PATCH_BOOL,NULL,0);
        if(m) ambient_bridge.zones[i]=(FxZoneGetter)patchlib_method_get_pointer(m);
    }
    ambient_bridge.ready=ambient_bridge.players && patchlib_array_length && patchlib_array_at &&
        patchlib_field_get_value && get_player && get_screen && get_width && get_height;
    note(ambient_bridge.ready ? "Global ambience sampler ready: one biome mote per interval." :
         "Global ambience sampler unavailable: player array or screen API missing.");
}
void fx_global_ambient_tick(uint64_t now) {
    if(!ambient_bridge.ready) return;
    if(ambient_bridge.last_sample && now>=ambient_bridge.last_sample && now-ambient_bridge.last_sample<900) return;
    if(now<ambient_bridge.last_sample) ambient_bridge.last_sample=0;
    ambient_bridge.last_sample=now;
    if(!playing()) return;
    int id=get_player(m_player),width=get_width(m_width),height=get_height(m_height);
    if(id<0 || id>=255 || width<=0 || height<=0 || width>10000 || height>10000) return;
    void *array=NULL,*player=NULL;
    patchlib_field_get_value(ambient_bridge.players,NULL,&array);
    if(!array || (size_t)id>=patchlib_array_length(array) ||
       !patchlib_array_at(array,(size_t)id,&player) || !player) return;
    FxBiome biome=FX_BIOME_DEFAULT;
    if(fx_zone(player,6)) biome=FX_BIOME_UNDERWORLD;
    else if(fx_zone(player,0)) biome=FX_BIOME_SNOW;
    else if(fx_zone(player,1)) biome=FX_BIOME_HALLOW;
    else if(fx_zone(player,2)) biome=FX_BIOME_JUNGLE;
    else if(fx_zone(player,3)) biome=FX_BIOME_CORRUPTION;
    else if(fx_zone(player,4)) biome=FX_BIOME_CRIMSON;
    else if(fx_zone(player,5)) biome=FX_BIOME_DESERT;
    FxAmbientEvent event={get_screen(m_screen),width,height,biome,now};
    fx_global_ambient(&event);
}
void fx_global_ambient_reset(void) { ambient_bridge.last_sample=0; }
void fx_global_ambient_shutdown(void) { memset(&ambient_bridge,0,sizeof(ambient_bridge)); }
