#include "src/RelativeRules.h"
#include <cassert>
#include <cstdio>
int main() {
    for (int biome = 0; biome < 14; biome++)
        for (int p : {1, 2, 10, 100, 1000})
            for (int x = -30; x < 30; x++) {
                auto r = progression::regionRoll(123, x, -x);
                int e = progression::enemyLevel(p, biome, r, 456);
                assert(e >= 1);
                assert(e == progression::enemyLevel(p, biome, r, 456));
                if (progression::tier(biome) > 0)
                    assert(e > p);
                for (int k = 0; k < 100; k++) {
                    int g = progression::lootLevel(p, e, k);
                    assert(g >= 1 && g <= std::max(p, e));
                    if (e <= p)
                        assert(g <= p);
                }
            }
    assert(progression::tier(12) == 2);
    assert(progression::tier(7) == 0);
    bool higher = false, normal = false;
    for (int k = 0; k < 100; k++) {
        int l = progression::lootLevel(10, 15, k);
        higher |= l > 10;
        normal |= l <= 10;
    }
    assert(higher && normal);
    puts("Relative biome and loot rules passed");
}
