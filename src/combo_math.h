#ifndef COMBATFX_COMBO_MATH_H
#define COMBATFX_COMBO_MATH_H
#include <stdbool.h>
#include <stdint.h>
#include <string.h>
#include <math.h>
#define FX_PI 3.14159265358979323846f
#define FX_WOODEN_SWORD 24
#define FX_COMBO_RESET_MS 800
#define FX_COMBO_NPCS 1024
#define FX_SWEEP_SAMPLES 7

typedef struct { float x, y; } FxPoint;
typedef struct { int x, y, w, h; } FxRect;
typedef struct { FxPoint hand, tip; } FxBlade;
typedef struct {
    bool valid, running, idle;
    unsigned stage, next;
    int slot;
    uint64_t idle_since;
    uint8_t hit[FX_COMBO_NPCS / 8];
} FxCombo;
static inline void fx_combo_reset(FxCombo *c) { memset(c, 0, sizeof(*c)); c->slot = -1; }
static inline void fx_combo_begin(FxCombo *c, int slot, uint64_t now) {
    if (!c->valid || c->slot != slot || (c->idle && (now < c->idle_since || now - c->idle_since >= FX_COMBO_RESET_MS)))
        fx_combo_reset(c);
    c->stage = c->next;
    c->next = (c->stage + 1) % 3;
    c->slot = slot; c->valid = c->running = true; c->idle = false;
    memset(c->hit, 0, sizeof(c->hit));
}
static inline void fx_combo_idle(FxCombo *c, uint64_t now) {
    c->running = false;
    if (!c->idle) { c->idle = true; c->idle_since = now; }
    if (now < c->idle_since || now - c->idle_since >= FX_COMBO_RESET_MS) fx_combo_reset(c);
}
static inline bool fx_combo_has_hit(const FxCombo *c, int npc) {
    return npc < 0 || npc >= FX_COMBO_NPCS || (c->hit[(unsigned)npc / 8] & (1u << ((unsigned)npc % 8))) != 0;
}
static inline void fx_combo_mark_hit(FxCombo *c, int npc) {
    if (npc >= 0 && npc < FX_COMBO_NPCS) c->hit[(unsigned)npc / 8] |= (uint8_t)(1u << ((unsigned)npc % 8));
}
static inline float fx_clamp(float v, float lo, float hi) { return v < lo ? lo : v > hi ? hi : v; }
static inline float fx_swing_progress(int remaining, int maximum) {
    return maximum > 1 ? fx_clamp((float)(maximum - remaining) / (float)(maximum - 1), 0, 1) : 1;
}
static inline bool fx_swing_active(float p) { return p >= 0.16f && p <= 0.82f; }
// Short preparation, accelerated cut, then visible recovery; each cut has its own rhythm.
static inline float fx_swing_fraction(unsigned stage, float p) {
    static const float start[] = {0.26f,0.22f,0.18f};
    static const float finish[] = {0.76f,0.72f,0.62f};
    stage %= 3;
    return fx_clamp((p-start[stage])/(finish[stage]-start[stage]),0,1);
}
static inline float fx_swing_angle(unsigned stage, float p) {
    static const float begin[] = {-1.15f, -1.35f, 1.35f};
    static const float end[] = {1.15f, 1.35f, -1.35f};
    stage %= 3;
    float q = fx_swing_fraction(stage,p);
    q = q * q * (3 - 2 * q);
    return begin[stage] + (end[stage] - begin[stage]) * q;
}
// Side-view projection distinguishes horizontal sweeps from an overhead chop.
static inline float fx_project_angle(unsigned stage, float angle) {
    return stage % 3 != 0 ? angle : atan2f(sinf(angle) * 0.30f, cosf(angle));
}
static inline float fx_project_length(unsigned stage, float angle) {
    if (stage % 3 != 0) return 1;
    float c = cosf(angle), s = sinf(angle) * 0.30f;
    return sqrtf(c*c + s*s);
}
static inline float fx_draw_lift(float p) {
    float q = fx_clamp(p / 0.26f, 0, 1); return q*q*(3-2*q);
}
static inline float fx_world_angle(float local_angle, int direction) { return direction > 0 ? local_angle : FX_PI - local_angle; }
// Clip the blade segment against an expanded NPC rectangle (slab intersection).
static inline bool fx_blade_intersects(FxBlade s, FxRect r, float padding) {
    if (r.w <= 0 || r.h <= 0 || !isfinite(s.hand.x) || !isfinite(s.hand.y) || !isfinite(s.tip.x) || !isfinite(s.tip.y)) return false;
    float lo = 0, hi = 1;
    const float origin[] = {s.hand.x, s.hand.y};
    const float delta[] = {s.tip.x - s.hand.x, s.tip.y - s.hand.y};
    const float minimum[] = {(float)r.x - padding, (float)r.y - padding};
    const float maximum[] = {(float)r.x + r.w + padding, (float)r.y + r.h + padding};
    for (unsigned i = 0; i < 2; ++i) {
        if (fabsf(delta[i]) < 0.00001f) { if (origin[i] < minimum[i] || origin[i] > maximum[i]) return false; }
        else {
            float a = (minimum[i] - origin[i]) / delta[i], b = (maximum[i] - origin[i]) / delta[i];
            if (a > b) { float swap = a; a = b; b = swap; }
            if (a > lo) lo = a;
            if (b < hi) hi = b;
            if (lo > hi) return false;
        }
    }
    return true;
}
static inline FxRect fx_sweep_bounds(const FxBlade *blades, unsigned count, float padding) {
    float x0 = blades[0].hand.x, x1 = x0, y0 = blades[0].hand.y, y1 = y0;
    for (unsigned i = 0; i < count; ++i) {
        const FxPoint points[] = {blades[i].hand, blades[i].tip};
        for (unsigned j = 0; j < 2; ++j) {
            x0 = fminf(x0, points[j].x); x1 = fmaxf(x1, points[j].x);
            y0 = fminf(y0, points[j].y); y1 = fmaxf(y1, points[j].y);
        }
    }
    int x = (int)floorf(x0 - padding), y = (int)floorf(y0 - padding);
    return (FxRect){x, y, (int)ceilf(x1 + padding) - x, (int)ceilf(y1 + padding) - y};
}
#endif
