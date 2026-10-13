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
                    int g = progression::lootLevel(p, e, biome, k);
                    assert(g >= 1 && g <= std::max(p, e));
                    assert(g <= progression::maximumLootLevel(p));
                    if (e <= p || progression::tier(biome) == 0)
                        assert(g <= p);
                }
            }
    assert(progression::tier(12) == 2);
    assert(progression::tier(7) == 0);
    for (int biome : {4, 5, 6, 9, 10, 11, 12, 13}) {
        bool higher = false, normal = false;
        for (int k = 0; k < 100; k++) {
            int l = progression::lootLevel(10, 15, biome, k);
            higher |= l > 10;
            normal |= l <= 10;
        }
        assert(higher && normal);
    }
    for (int k = 0; k < 100; k++) {
        // A level-1 starter inventory seed at the native origin must not inherit
        // tens of thousands of levels from the character's distant spawn region.
        assert(progression::ownedGearLevel(1, 0, 47500, true, k) == 1);
        assert(progression::ownedGearLevel(1, 0, 47500, false, k) == 1);
        assert(progression::ownedGearLevel(1, 47500, 47500, true, k) == 1);
        // Legitimate fixed gear and above-player loot survive session registration.
        assert(progression::ownedGearLevel(50, 50, 47500, true, k) == 50);
        assert(progression::ownedGearLevel(50, 65, 47500, true, k) == 65);
        assert(progression::ownedGearLevel(50, 65, 65, true, k) == 65);
        int ceiling = progression::maximumLootLevel(50);
        assert(progression::ownedGearLevel(50, ceiling, ceiling, true, k) == ceiling);
        // A high record without both artifact markers is preserved.
        assert(progression::ownedGearLevel(1, 47500, 47500, false, k) == 47500);
        assert(progression::ownedGearLevel(1, 47500, 47501, true, k) == 47500);
        int owned = progression::ownedGearLevel(50, 0, 47500, true, k);
        assert(owned >= 48 && owned <= 50);
        // Easy-biome variation can put an enemy above the player; its gear cannot.
        assert(progression::lootLevel(1, 2, 0, k) == 1);
        assert(progression::lootLevel(10, 15, 7, k) <= 10);
    }
    puts("Relative biome, loot and owned-gear migration rules passed");
}
