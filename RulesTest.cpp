#include "src/RelativeRules.h"
#include <array>
#include <cassert>
#include <cstdio>

static void testEnemyLevelsAndLootBounds() {
    const std::array<int, 14> expectedTiers = {0, 0, 0, 0, 2, 1, 1, 0, 0, 2, 1, 1, 2, 2};
    for (int biome = 0; biome < 14; biome++) {
        assert(progression::tier(biome) == expectedTiers[biome]);
        for (int p : {1, 2, 10, 100, 1000, 1000000})
            for (int x = -30; x < 30; x++) {
                auto r = progression::regionRoll(123, x, -x);
                int e = progression::enemyLevel(p, biome, r, 456);
                assert(e >= 1);
                assert(e == progression::enemyLevel(p, biome, r, 456));
                if (progression::tier(biome) > 0)
                    assert(e > p);
                for (bool dungeon : {false, true})
                    for (int k = 0; k < 100; k++) {
                        int g = progression::lootLevel(p, e, biome, k, dungeon);
                        assert(g >= p && g <= progression::maximumLootLevel(p));
                        if (e > p)
                            assert(g <= e);
                        else
                            assert(g <= p + progression::tier(biome) + 1);
                    }
            }
    }
    assert(progression::tier(-1) == 0);
    assert(progression::tier(14) == 0);
    assert(progression::boundedPlayerLevel(0) == 1);
    assert(progression::boundedPlayerLevel(1000001) == 1000000);
}

static void testRarityDistributions() {
    const int expected[2][3][5] = {{{10, 20, 50, 15, 5}, {5, 10, 25, 45, 15}, {3, 7, 15, 30, 45}},
                                   {{4, 11, 35, 30, 20}, {2, 5, 12, 46, 35}, {1, 4, 8, 32, 55}}};
    for (int biome = 0; biome < 14; biome++) {
        std::array<int, 5> counts[2] = {};
        for (int dungeon = 0; dungeon < 2; dungeon++) {
            for (uint64_t roll = 0; roll < 100; roll++) {
                int rarity = progression::lootRarity(biome, roll, dungeon != 0);
                assert(rarity >= 0 && rarity <= 4);
                assert(rarity == progression::lootRarity(biome, roll + 100, dungeon != 0));
                counts[dungeon][rarity]++;
            }
            int t = progression::tier(biome);
            for (int rarity = 0; rarity < 5; rarity++) {
                assert(counts[dungeon][rarity] == expected[dungeon][t][rarity]);
                assert(counts[dungeon][rarity] > 0);
            }
            // The modes are 3, 4 and 5 stars in easy, moderate and hard regions,
            // including their dungeon reward distributions.
            int mode = t + 2;
            for (int rarity = 0; rarity < 5; rarity++)
                if (rarity != mode)
                    assert(counts[dungeon][mode] > counts[dungeon][rarity]);
        }
        // Dungeons increase both 4-star and 5-star chances, and every cutoff's
        // upper tail grows: the full distribution shifts toward stronger gear.
        assert(counts[1][3] > counts[0][3]);
        assert(counts[1][4] > counts[0][4]);
        for (int cutoff = 1; cutoff < 5; cutoff++) {
            int outdoorTail = 0, dungeonTail = 0;
            for (int rarity = cutoff; rarity < 5; rarity++) {
                outdoorTail += counts[0][rarity];
                dungeonTail += counts[1][rarity];
            }
            assert(dungeonTail > outdoorTail);
        }
    }
}

static void testLootLevelChances() {
    const int biomes[3] = {0, 5, 4};
    int aboveCounts[2][3] = {};
    for (int t = 0; t < 3; t++) {
        for (int dungeon = 0; dungeon < 2; dungeon++) {
            bool matchedPlayer = false, abovePlayer = false;
            for (uint64_t roll = 0; roll < 10000; roll++) {
                int level = progression::lootLevel(10, 15, biomes[t], roll, dungeon != 0);
                assert(level >= 10 && level <= 15);
                matchedPlayer |= level == 10;
                abovePlayer |= level > 10;
                aboveCounts[dungeon][t] += level > 10;
                // A source only one level above the player can never drop a
                // higher level, regardless of biome or reward context.
                int nearLevel = progression::lootLevel(10, 11, biomes[t], roll, dungeon != 0);
                assert(nearLevel == 10 || nearLevel == 11);
                for (int source : {0, 9, 10}) {
                    int reward = progression::lootLevel(10, source, biomes[t], roll, dungeon != 0);
                    assert(reward >= 10 && reward <= 11 + t);
                }
            }
            assert(matchedPlayer && abovePlayer);
        }
        assert(aboveCounts[1][t] > aboveCounts[0][t]);
    }
    for (int dungeon = 0; dungeon < 2; dungeon++) {
        assert(aboveCounts[dungeon][0] < aboveCounts[dungeon][1]);
        assert(aboveCounts[dungeon][1] < aboveCounts[dungeon][2]);
    }
    // Even easy level-1 regions can grant level-2 new loot; registered starter
    // inventory is checked separately and must always remain level 1.
    bool easyHigher = false, easyEqual = false;
    for (int roll = 0; roll < 100; roll++) {
        int level = progression::lootLevel(1, 1, 0, roll);
        assert(level == 1 || level == 2);
        easyHigher |= level == 2;
        easyEqual |= level == 1;
    }
    assert(easyHigher && easyEqual);
}

static void testOwnedGearRegistrationAndMigration() {
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
        assert(progression::ownedGearLevel(50, 0, 47500, true, k) == 50);
    }
}

int main() {
    for (int type : {4, 7, 8, 12, 15, 40, 41, 42, 43, 44, 45, 46, 47})
        assert(progression::dungeonType(type));
    for (int type : {-1, 0, 5, 6, 9, 38, 48})
        assert(!progression::dungeonType(type));
    testEnemyLevelsAndLootBounds();
    testRarityDistributions();
    testLootLevelChances();
    testOwnedGearRegistrationAndMigration();
    puts("Relative biome, star rarity, loot level and owned-gear migration rules passed");
}
