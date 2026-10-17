#ifndef COMBATFX_LIMITS_H
#define COMBATFX_LIMITS_H
#include <stdint.h>
#include <stdbool.h>
#include <string.h>
#define FX_SLOTS 256
#define FX_RATE 90.0
#define FX_CAPACITY 12.0
#define FX_COOLDOWN_MS 65

typedef struct { uint64_t at; int npc; bool used; } FxRecent;
typedef struct {
    double tokens;
    uint64_t refill;
    bool initialized;
    FxRecent recent[FX_SLOTS];
} FxBudget;
static inline void fx_budget_reset(FxBudget *b) { memset(b, 0, sizeof(*b)); }
static inline bool fx_budget_take(FxBudget *b, uint64_t now, int npc, unsigned count) {
    if (npc < 0 || count == 0 || count > 12) return false;
    if (!b->initialized || now < b->refill) {
        fx_budget_reset(b); b->initialized = true; b->tokens = FX_CAPACITY; b->refill = now;
    }
    uint64_t elapsed = now - b->refill;
    b->tokens += (double)elapsed * FX_RATE / 1000.0;
    if (b->tokens > FX_CAPACITY) b->tokens = FX_CAPACITY;
    b->refill = now;
    FxRecent *r = &b->recent[(unsigned)npc % FX_SLOTS];
    if (r->used && r->npc == npc && now >= r->at && now - r->at < FX_COOLDOWN_MS) return false;
    if (b->tokens < count) return false;
    b->tokens -= count; r->used = true; r->npc = npc; r->at = now;
    return true;
}
#endif
