#pragma once
// Pure level rules for player-relative progression (see RelativeScaling.cpp).
// No game dependencies; tested by RulesTest.cpp.
#include <cstdint>
#include <algorithm>
namespace progression {
// 64-bit finaliser (SplitMix64) used for all deterministic hashes.
inline uint64_t mix(uint64_t v) {
    v ^= v >> 30;
    v *= 0xbf58476d1ce4e5b9ULL;
    v ^= v >> 27;
    v *= 0x94d049bb133111ebULL;
    return v ^ (v >> 31);
}
// Stable random value for region (x, y) in a world.
inline uint64_t regionRoll(int seed, int x, int y) {
    return mix(uint32_t(seed) ^ mix(uint32_t(x)) ^
               mix(uint64_t(uint32_t(y)) + 0x9e3779b97f4a7c15ULL));
}
// Difficulty tier of a native biome ID: 0 normal, 1 harder, 2 hardest.
inline int tier(int biome) {
    switch (biome) {
    case 4:
    case 9:
    case 12:
    case 13:
        return 2;
    case 5:
    case 6:
    case 10:
    case 11:
        return 1;
    default:
        return 0;
    }
}
inline bool dungeonType(int type) {
    switch (type) {
    case 4:
    case 7:
    case 8:
    case 12:
    case 15:
    case 40:
    case 41:
    case 42:
    case 43:
    case 44:
    case 45:
    case 46:
    case 47:
        return true;
    default:
        return false;
    }
}
inline int boundedPlayerLevel(int player) {
    return std::max(1, std::min(player, 1000000));
}
inline int enemyLevel(int player, int biome, uint64_t region, uint64_t entity) {
    player = boundedPlayerLevel(player);
    int t = tier(biome);
    // Region roll stays fixed across reloads; creature variation is only a few levels.
    int percent = (t == 0 ? 90 : t == 1 ? 108 : 125) + int(region % 9);
    int result = (player * percent + 50) / 100 + int(mix(entity) % 3) - 1;
    return std::max(t ? player + t : 1, result);
}
inline int maximumLootLevel(int player) {
    player = boundedPlayerLevel(player);
    // Hard biome's source-free loot band or its strongest relative enemy.
    return std::max(player + 3, (player * 133 + 50) / 100 + 1);
}
inline int lootRarity(int biome, uint64_t roll, bool dungeon = false) {
    // Native rarities 0..4 are the game's 1..5 stars. Every tier stays possible,
    // with 3/4/5 stars most likely in easy/moderate/hard outdoor regions.
    static constexpr int weights[2][3][5] = {
        {{10, 20, 50, 15, 5}, {5, 10, 25, 45, 15}, {3, 7, 15, 30, 45}},
        {{4, 11, 35, 30, 20}, {2, 5, 12, 46, 35}, {1, 4, 8, 32, 55}}};
    int percentile = int(roll % 100);
    for (int rarity = 0; rarity < 4; rarity++) {
        int weight = weights[dungeon ? 1 : 0][tier(biome)][rarity];
        if (percentile < weight)
            return rarity;
        percentile -= weight;
    }
    return 4;
}
inline int lootLevel(int player, int enemy, int biome, uint64_t roll, bool dungeon = false) {
    player = boundedPlayerLevel(player);
    int t = tier(biome);
    // Most loot matches the player; harder biomes raise the above-level chance
    // from 10% to 20% to 30%. Dungeon rewards add five percentage points.
    int aboveChance = 10 + 10 * t + (dungeon ? 5 : 0);
    if (mix(roll) % 100 >= uint64_t(aboveChance))
        return player;
    // Above-player enemies bound their own drops. Chests and other rewards
    // without such a source have a small, biome-dependent 1/2/3-level band.
    int ceiling = enemy > player ? std::min(enemy, maximumLootLevel(player)) : player + t + 1;
    return player + 1 + int(mix(roll + 1) % uint64_t(ceiling - player));
}
inline bool legacyDistanceArtifact(int player, int saved, int legacy, bool /*originRegion*/) {
    // Relative loot never exceeds maximumLootLevel(player) (enemy-level loot is
    // capped to it too), so owned gear far above it is a pre-relative distance
    // level. The old distance level is measured from the player's current base
    // region, so a record saved from another region no longer matches `legacy`:
    // starter gear kept ~54,000 levels and a level-1 character ~262k health.
    int ceiling = maximumLootLevel(player);
    return saved > ceiling && (saved == legacy || saved > ceiling * 4 + 100);
}
inline int ownedGearLevel(int player, int saved, int legacy, bool originRegion, uint64_t /*roll*/) {
    if (saved > 0 && !legacyDistanceArtifact(player, saved, legacy, originRegion))
        return saved;
    // Initial inventory registration is separate from newly generated loot.
    return boundedPlayerLevel(player);
}
} // namespace progression
