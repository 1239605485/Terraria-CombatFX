#include <assert.h>
#include "effect_budget.h"
int main(void) {
    FxBudget b = {0};
    assert(fx_budget_take(&b, 0, 1, 3));
    assert(!fx_budget_take(&b, 64, 1, 3));
    assert(fx_budget_take(&b, 65, 1, 3));
    fx_budget_reset(&b);
    for (int i = 0; i < 4; ++i) assert(fx_budget_take(&b, 100, i, 3));
    assert(!fx_budget_take(&b, 100, 10, 3));
    assert(!fx_budget_take(&b, 132, 10, 3));
    assert(fx_budget_take(&b, 134, 10, 3));
    // A long idle period cannot accumulate unbounded bursts.
    assert(fx_budget_take(&b, 100000, 11, 12));
    assert(!fx_budget_take(&b, 100000, 12, 1));
    assert(!fx_budget_take(&b, 100000, -1, 3));
    assert(!fx_budget_take(&b, 100000, 3, 13));
    // Monotonic clock reset is handled without unsigned-underflow cooldowns.
    assert(fx_budget_take(&b, 1, 11, 3));
    fx_budget_reset(&b);
    unsigned emitted = 0;
    for (unsigned t = 0; t < 1000; ++t)
        if (fx_budget_take(&b, t, (int)t, 3)) emitted += 3;
    assert(emitted <= 102);
    return 0;
}
