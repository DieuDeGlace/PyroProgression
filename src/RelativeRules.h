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
// Enemy level: 90% (tier 0), 108% or 125% of the player level, +0..8% per region,
// +-1 per creature; harder tiers are always above the player.
inline int enemyLevel(int player, int biome, uint64_t region, uint64_t entity) {
    player = std::max(1, std::min(player, 1000000));
    int t = tier(biome);
    // Region roll stays fixed across reloads; creature variation is only a few levels.
    int percent = (t == 0 ? 90 : t == 1 ? 108 : 125) + int(region % 9);
    int result = (player * percent + 50) / 100 + int(mix(entity) % 3) - 1;
    return std::max(t ? player + t : 1, result);
}
// Level of a gear drop: usually the player level minus 0..2.
inline int lootLevel(int player, int enemy, uint64_t roll) {
    player = std::max(1, std::min(player, 1000000));
    int ordinary = std::max(1, player - int(roll % 3));
    // One in three drops from an above-level enemy can be above the player.
    if (enemy > player && mix(roll) % 3 == 0)
        return player + 1 + int(mix(roll + 1) % uint64_t(enemy - player));
    return ordinary;
}
} // namespace progression
