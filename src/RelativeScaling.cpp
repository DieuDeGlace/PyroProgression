// Player-relative progression: enemy levels follow the player's level (scaled
// up in harder biomes, with a fixed per-region roll), and every piece of gear is
// given a fixed level when first seen (at drop time, or its legacy level for
// gear owned before this mod). Levels are persisted in
// Mods/PyroRelativeLevels.state, keyed by world, character and item.
//
// Optional "discovery lock" (INI LockDiscoveredRegions=1 or the
// DISCOVERY_LOCK_DEFAULT build flag): a region keeps the player level it was
// first visited at, instead of following the player.
#include "utility.h"
#include "RelativeRules.h"
#include "NativeLootAdapter.h"
#include <unordered_map>
#include <fstream>
#include <mutex>
#include <cstdio>
#include <cmath>
#ifndef DISCOVERY_LOCK_DEFAULT
#define DISCOVERY_LOCK_DEFAULT 0
#endif
namespace {
// RVAs in the supported cubeworld.exe.
constexpr uintptr_t BIOME_AT_RVA = 0x2b9080;  // int (World*, regionX, regionY)
constexpr uintptr_t DROP_ITEM_RVA = 0x2a5b70; // Hooked: drops an item into the world.
constexpr int MAX_STORED_LEVEL = 10000000;    // Saved levels above this are ignored.

// SDK allocations use the game's allocator, available after ModPreInitialize.
// Defer containers until then rather than allocating during DLL attachment.
struct Data {
    std::recursive_mutex gate;
    std::unordered_map<uint64_t, int> gear, regions, biomes, rarities;
};
Data &data() {
    static Data value;
    return value;
}
#define gate data().gate
#define gear data().gear
#define regions data().regions
#define biomes data().biomes
#define rarities data().rarities
cube::Creature *sessionPlayer = nullptr;
cube::World *sessionWorld = nullptr;
uint64_t identity = 0; // Hash of world seed, world name and character name.
bool loaded = false, locked = DISCOVERY_LOCK_DEFAULT != 0;
const char *statePath = "Mods/PyroRelativeLevels.state";

int playerLevel() {
    auto g = cube::GetGame();
    auto p = g && g->world ? g->GetPlayer() : nullptr;
    return p ? std::max(1, int(p->entity_data.level)) : 1;
}
// Gear categories (3..9) get a fixed relative level; other items keep the legacy one.
bool isGear(const cube::Item *i) {
    return i->category >= 3 && i->category <= 9;
}
// Block coordinate (floor division) of a dot coordinate.
int blockFromDots(int64_t n) {
    return int(n / 65536 - (n < 0 && n % 65536 != 0));
}
uint64_t itemKey(cube::Item *i) {
    uint64_t h = identity;
    for (uint64_t v : {uint64_t(uint8_t(i->category)), uint64_t(uint32_t(i->id)),
                       uint64_t(i->modifier), uint64_t(uint8_t(i->material))})
        h = progression::mix(h ^ v);
    // Rarity and spirits can change when upgrading an existing item; its level stays fixed.
    return h;
}
uint64_t regionKey(IntVector2 r) {
    return progression::mix(identity ^ progression::regionRoll(0, r.x, r.y));
}
// Appends one record ('G' gear or 'R' region) to the state file.
void save(char kind, uint64_t key, int level) {
    std::ofstream out(statePath, std::ios::app);
    out << kind << ' ' << key << ' ' << level << '\n';
}
// Records a gear level the first time an item is seen; later calls keep the first level.
void registerGear(cube::Item *i, int level) {
    if (!i || !isGear(i))
        return;
    auto k = itemKey(i);
    if (gear().emplace(k, std::max(1, level)).second)
        save('G', k, std::max(1, level));
}
void assignGear(cube::Item *i, int level) {
    auto k = itemKey(i);
    auto it = gear.find(k);
    if (it == gear.end() || it->second != level) {
        gear[k] = level;
        save('G', k, level);
    }
}
int legacy(cube::Item *i) {
    return 1 + LEVELS_PER_REGION * GetRegionDistance(i->region) +
           GetLevelVariation(i->modifier, LEVELS_PER_REGION);
}
template <typename Visit> void visitOwnedGear(cube::Creature *p, Visit visit) {
    auto &e = p->entity_data.equipment;
    for (auto i : {&e.neck, &e.chest, &e.feet, &e.hands, &e.shoulder, &e.weapon_left,
                   &e.weapon_right, &e.ring_left, &e.ring_right})
        if (i->category >= 3 && i->category <= 9)
            visit(i);
    for (auto &tab : p->inventory_tabs)
        for (auto &stack : tab)
            if (stack.quantity > 0 && stack.item.category >= 3 && stack.item.category <= 9)
                visit(&stack.item);
}
void registerOwnedGear(cube::Item *i, int player) {
    if (!i || i->category < 3 || i->category > 9)
        return;
    auto k = itemKey(i);
    auto it = gear.find(k);
    int saved = it == gear.end() ? 0 : it->second;
    int level = progression::ownedGearLevel(player, saved, saved ? legacy(i) : 0,
                                            i->region == IntVector2(0, 0), k);
    if (it == gear.end())
        registerGear(i, level);
    else if (level != saved) {
        it->second = level;
        save('G', k, level);
    }
}
IntVector2 creatureRegion(cube::Creature *c) {
    auto r = c->entity_data.current_region;
    if (r == IntVector2(0, 0)) {
        auto block = [](int64_t n) { return int(n / 65536 - (n < 0 && n % 65536 != 0)); };
        auto g = cube::GetGame();
        g->world->GetRegion(&r, block(c->entity_data.position.x), block(c->entity_data.position.y));
    }
    return r;
}
void session() {
    auto g = cube::GetGame();
    auto p = g && g->world ? g->GetPlayer() : nullptr;
    if (!p) {
        sessionPlayer = nullptr;
        sessionWorld = nullptr;
        return;
    }
    if (!loaded) {
        std::ifstream in(statePath);
        char kind;
        uint64_t key;
        int l;
        while (in >> kind >> key >> l) {
            if (kind == 'T' && l >= 0 && l <= 4)
                rarities[key] = l;
            else if (l > 0 && l <= 10000000) {
                if (kind == 'G')
                    gear[key] = l;
                else if (kind == 'R')
                    regions[key] = l;
            }
        }
        loaded = true;
    }
    uint64_t nextIdentity = progression::mix(uint32_t(g->world->seed));
    for (unsigned char c : g->world->world_name)
        nextIdentity = progression::mix(nextIdentity ^ c);
    for (int n = 0; n < 16 && p->entity_data.name[n]; n++)
        nextIdentity = progression::mix(nextIdentity ^ uint8_t(p->entity_data.name[n]));
    // The menu preview can be reused when a new character is confirmed. Its
    // final name and world must be considered even if the pointers stay equal.
    if (p == sessionPlayer && g->world == sessionWorld && nextIdentity == identity)
        return;
    sessionPlayer = p;
    sessionWorld = g->world;
    identity = nextIdentity;
    biomes.clear();
    locked = DISCOVERY_LOCK_DEFAULT != 0 ||
             GetPrivateProfileIntW(L"PyroProgression", L"LockDiscoveredRegions", 0,
                                   L".\\Mods\\PyroProgression.ini") != 0;
    int player = playerLevel();
    visitOwnedGear(p, [player](cube::Item *item) { registerOwnedGear(item, player); });
}
// Native biome of a region (cached per session).
int biome(IntVector2 r) {
    auto key = progression::regionRoll(0, r.x, r.y);
    auto it = biomes().find(key);
    if (it != biomes().end())
        return it->second;
    auto g = cube::GetGame();
    int b = ((int (*)(cube::World *, int, int))CWOffset(BIOME_AT_RVA))(g->world, r.x, r.y);
    biomes()[key] = b;
    return b;
}
// The player level enemies in region r scale from.
int basis(IntVector2 r) {
    auto it = regions().find(regionKey(r));
    return locked && it != regions().end() ? it->second : playerLevel();
}

// --- Item drop hook -----------------------------------------------------------
using DropFn = void (*)(cube::World *, cube::Item *, LongVector3 *, float, float, int,
                        cube::Creature *, uint64_t);
DropFn originalDrop = nullptr;
struct NativeLocation {
    int type;
    unsigned char gridFlag, padding[3];
    int subtype, radius, x, y;
    float distanceSquared, influence;
};
static_assert(sizeof(NativeLocation) == 32);
int blockCoordinate(int64_t n) {
    return int(n / 65536 - (n < 0 && n % 65536 != 0));
}
struct LootContext {
    int biome = 0;
    bool dungeon = false;
};
LootContext lootContext(cube::World *w, const LongVector3 *pos) {
    LootContext result;
    if (!pos)
        return result;
    int x = blockCoordinate(pos->x), y = blockCoordinate(pos->y);
    IntVector2 region;
    w->GetRegion(&region, x, y);
    result.biome = biome(region);
    NativeLocation location{};
    reinterpret_cast<void (*)(cube::World *, int, int, NativeLocation *)>(CWOffset(0x35ce0))(
        w, x, y, &location);
    // The nearest descriptor is returned outside the actual location as well.
    if (!std::isfinite(location.influence) || location.influence <= 0)
        return result;
    result.dungeon = progression::dungeonType(location.type);
    if (!result.dungeon && (location.type == 5 || location.type == 9)) {
        float surface = reinterpret_cast<float (*)(cube::World *, int, int, int)>(
            CWOffset(0x35ea0))(w, x, y, 0);
        if (std::isfinite(surface) && double(pos->z) / 65536 < surface - 4) {
            auto floorGrid = [](int n) { return n / 1024 - (n < 0 && n % 1024 != 0); };
            int underground =
                reinterpret_cast<int (*)(cube::World *, int, int, int, bool)>(CWOffset(0x2aa270))(
                    w, floorGrid(x), floorGrid(y), location.type, location.gridFlag != 0);
            result.dungeon = progression::dungeonType(underground);
        }
    }
    return result;
}
bool ownsGear(uint64_t key) {
    bool found = false;
    visitOwnedGear(sessionPlayer, [&](cube::Item *item) { found |= itemKey(item) == key; });
    return found;
}
void drop(cube::World *w, cube::Item *i, LongVector3 *pos, float a, float b, int amount,
          cube::Creature *source, uint64_t owner) {
    int finalRarity = -1;
    {
        std::lock_guard<std::recursive_mutex> guard(gate());
        session();
        // Manual-drop dispatch can remove the stack before reaching this hook.
        // Its player source still identifies existing gear that must not reroll.
        bool playerDrop =
            source && (source == sessionPlayer || source->entity_data.hostility_type ==
                                                      cube::Creature::EntityBehaviour::Player);
        if (sessionPlayer && !playerDrop && i && i->category >= 3 && i->category <= 9 &&
            !ownsGear(itemKey(i))) {
            bool hostileSource = source && source->entity_data.hostility_type ==
                                               cube::Creature::EntityBehaviour::Hostile;
            auto context = lootContext(w, hostileSource ? &source->entity_data.position : pos);
            int player = playerLevel();
            int enemy = player;
            if (hostileSource) {
                enemy = GetCreatureLevel(source);
                context.biome = biome(creatureRegion(source));
            }
            auto key = itemKey(i);
            auto existing = rarities.find(key);
            if (existing == rarities.end()) {
                finalRarity = progression::lootRarity(context.biome, key, context.dungeon);
                assignGear(
                    i, progression::lootLevel(player, enemy, context.biome, key, context.dungeon));
                rarities.emplace(key, finalRarity);
                save('T', key, finalRarity);
            } else {
                finalRarity = existing->second;
                auto level = gear.find(key);
                // Reused native seeds can appear in a later reward. Gear the
                // player already owns stays fixed; a new drop cannot be below P.
                assignGear(i, level == gear.end() ? player : std::max(player, level->second));
            }
        }
    }
    progression::FinalDropRarityScope rarityScope(finalRarity);
    originalDrop(w, i, pos, a, b, amount, source, owner);
}
using CreateCharacterFn = void (*)(cube::Game *);
CreateCharacterFn originalCreateCharacter = nullptr;
void createCharacter(cube::Game *g) {
    originalCreateCharacter(g);
    std::lock_guard<std::recursive_mutex> guard(gate);
    sessionPlayer = nullptr;
    session();
    if (sessionPlayer && playerLevel() == 1) {
        // Starter modifiers repeat for an account. Recreating a same-named
        // character must not inherit an old distance-based or upgraded record.
        visitOwnedGear(sessionPlayer, [](cube::Item *item) { assignGear(item, 1); });
    }
}
bool installEntryHook(unsigned char *site, const unsigned char *expected, size_t size, void *target,
                      void **original) {
    if (size < 14 || size > 32 || memcmp(site, expected, size) != 0)
        return false;
    auto trampoline = static_cast<unsigned char *>(
        VirtualAlloc(nullptr, size + 14, MEM_RESERVE | MEM_COMMIT, PAGE_EXECUTE_READWRITE));
    if (!trampoline)
        return false;
    memcpy(trampoline, site, size);
    unsigned char jump[14] = {0xff, 0x25, 0, 0, 0, 0};
    auto resume = site + size;
    memcpy(jump + 6, &resume, 8);
    memcpy(trampoline + size, jump, 14);
    DWORD prior;
    if (!VirtualProtect(site, size, PAGE_EXECUTE_READWRITE, &prior)) {
        VirtualFree(trampoline, 0, MEM_RELEASE);
        return false;
    }
    *original = trampoline;
    memcpy(jump + 6, &target, 8);
    memcpy(site, jump, 14);
    memset(site + 14, 0x90, size - 14);
    VirtualProtect(site, size, prior, &prior);
    FlushInstructionCache(GetCurrentProcess(), site, size);
    return true;
}
} // namespace

// Level shown/used for an item: the fixed relative level for gear, legacy otherwise.
int RelativeItemLevel(cube::Item *i) {
    std::lock_guard<std::recursive_mutex> guard(gate());
    session();
    if (!i)
        return 1;
    if (!sessionPlayer)
        return 1;
    if (!isGear(i))
        return legacy(i);
    auto k = itemKey(i);
    auto it = gear().find(k);
    if (it != gear().end())
        return it->second;
    int player = playerLevel();
    registerGear(i, progression::lootLevel(player, player, 0, k));
    return gear[k];
}
// Level of a non-player, non-pet creature, relative to the player and its region.
int RelativeCreatureLevel(cube::Creature *c) {
    std::lock_guard<std::recursive_mutex> guard(gate());
    session();
    if (!c)
        return 1;
    if (!sessionPlayer ||
        c->entity_data.hostility_type == cube::Creature::EntityBehaviour::Player ||
        c->entity_data.hostility_type == cube::Creature::EntityBehaviour::Pet)
        return c->entity_data.level;
    auto g = cube::GetGame();
    IntVector2 r = creatureRegion(c);
    return progression::enemyLevel(basis(r), biome(r),
                                   progression::regionRoll(g->world->seed, r.x, r.y), c->id);
}
// Per tick: with the discovery lock on, records the level the player's current
// region was first visited at.
void RelativeTick(cube::Game *g) {
    std::lock_guard<std::recursive_mutex> guard(gate());
    session();
    if (!sessionPlayer || !g || !g->world)
        return;
    if (locked) {
        IntVector2 r;
        auto p = sessionPlayer->entity_data.position;
        g->world->GetRegion(&r, blockFromDots(p.x), blockFromDots(p.y));
        auto key = regionKey(r);
        if (regions().emplace(key, playerLevel()).second)
            save('R', key, playerLevel());
    }
}
// Hooks the native item drop function. Returns false (changing nothing) if its
// prologue is not exactly as expected.
bool SetupRelativeLoot() {
    auto site = (unsigned char *)CWOffset(DROP_ITEM_RVA);
    const unsigned char expected[14] = {0x48, 0x8b, 0xc4, 0x55, 0x56, 0x57, 0x41,
                                        0x54, 0x41, 0x55, 0x41, 0x56, 0x41, 0x57};
    if (memcmp(site, expected, 14) != 0)
        return false;
    auto createSite = static_cast<unsigned char *>(CWOffset(0x9ea10));
    const unsigned char createExpected[16] = {0x48, 0x8b, 0xc4, 0x55, 0x41, 0x54, 0x41, 0x55,
                                              0x41, 0x56, 0x41, 0x57, 0x48, 0x8d, 0x68, 0xa1};
    if (memcmp(createSite, createExpected, sizeof(createExpected)) != 0 ||
        !progression::SetupNativeFinalLoot())
        return false;
    return installEntryHook(site, expected, sizeof(expected), reinterpret_cast<void *>(&drop),
                            reinterpret_cast<void **>(&originalDrop)) &&
           installEntryHook(createSite, createExpected, sizeof(createExpected),
                            reinterpret_cast<void *>(&createCharacter),
                            reinterpret_cast<void **>(&originalCreateCharacter));
}
extern "C" void ModPreInitialize();
// Diagnostic export: writes the levels of nearby enemies and some owned items
// to .mod-work/PyroProgression/live-audit.txt.
EXPORT DWORD WINAPI AuditRelativeProgression(void *) {
    ModPreInitialize();
    std::lock_guard<std::recursive_mutex> guard(gate());
    auto g = cube::GetGame();
    if (!g || !g->world || !g->GetPlayer())
        return 0;
    session();
    std::ofstream out(".mod-work/PyroProgression/live-audit.txt");
    out << "Player " << playerLevel() << "; lock mode " << locked << "; API size "
        << sizeof(GenericMod) << "\n";
    int count = 0;
    for (auto c : g->world->creatures) {
        if (!c || c == g->GetPlayer() ||
            c->entity_data.hostility_type != cube::Creature::EntityBehaviour::Hostile)
            continue;
        auto r = c->entity_data.current_region;
        out << "Enemy " << c->id << " race " << c->entity_data.race << " stars "
            << c->entity_data.level << " biome " << biome(r) << " new numeric level "
            << RelativeCreatureLevel(c) << "\n";
        if (++count == 24)
            break;
    }
    auto e = &g->GetPlayer()->entity_data.equipment;
    for (auto i : {&e->weapon_left, &e->weapon_right, &e->chest})
        out << "Owned item " << int(i->category) << " legacy " << legacy(i) << " fixed "
            << RelativeItemLevel(i) << "\n";
    return 1;
}
