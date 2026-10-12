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
#include <unordered_map>
#include <fstream>
#include <mutex>
#include <cstdio>
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
    std::unordered_map<uint64_t, int> gear, regions, biomes;
};
Data &data() {
    static Data value;
    return value;
}
std::recursive_mutex &gate() {
    return data().gate;
}
std::unordered_map<uint64_t, int> &gear() { // Item key -> fixed level.
    return data().gear;
}
std::unordered_map<uint64_t, int> &regions() { // Region key -> discovery level.
    return data().regions;
}
std::unordered_map<uint64_t, int> &biomes() { // Region roll -> native biome (cache).
    return data().biomes;
}

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
// The item's level under the original (region-distance based) progression.
int legacy(cube::Item *i) {
    return 1 + LEVELS_PER_REGION * GetRegionDistance(i->region) +
           GetLevelVariation(i->modifier, LEVELS_PER_REGION);
}
void loadState() {
    std::ifstream in(statePath);
    char kind;
    uint64_t key;
    int l;
    while (in >> kind >> key >> l)
        if (l > 0 && l <= MAX_STORED_LEVEL) {
            if (kind == 'G')
                gear()[key] = l;
            else if (kind == 'R')
                regions()[key] = l;
        }
}
// Brings session state up to date: loads the state file once, and on a new
// player or world recomputes the identity, the lock setting, and registers the
// player's existing equipment and inventory at their legacy levels.
void session() {
    auto g = cube::GetGame();
    auto p = g && g->world ? g->GetPlayer() : nullptr;
    if (!p) {
        sessionPlayer = nullptr;
        sessionWorld = nullptr;
        return;
    }
    if (!loaded) {
        loadState();
        loaded = true;
    }
    if (p == sessionPlayer && g->world == sessionWorld)
        return;
    sessionPlayer = p;
    sessionWorld = g->world;
    biomes().clear();
    identity = progression::mix(uint32_t(g->world->seed));
    for (unsigned char c : g->world->world_name)
        identity = progression::mix(identity ^ c);
    for (int n = 0; n < 16 && p->entity_data.name[n]; n++)
        identity = progression::mix(identity ^ uint8_t(p->entity_data.name[n]));
    locked = DISCOVERY_LOCK_DEFAULT != 0 ||
             GetPrivateProfileIntW(L"PyroProgression", L"LockDiscoveredRegions", 0,
                                   L".\\Mods\\PyroProgression.ini") != 0;
    auto &e = p->entity_data.equipment;
    for (auto i : {&e.neck, &e.chest, &e.feet, &e.hands, &e.shoulder, &e.weapon_left,
                   &e.weapon_right, &e.ring_left, &e.ring_right})
        registerGear(i, legacy(i));
    for (auto &tab : p->inventory_tabs)
        for (auto &stack : tab)
            registerGear(&stack.item, legacy(&stack.item));
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
// Fixes the level of dropped gear (relative to the player and the hostile source).
void drop(cube::World *w, cube::Item *i, LongVector3 *pos, float a, float b, int amount,
          cube::Creature *source, uint64_t owner) {
    {
        std::lock_guard<std::recursive_mutex> guard(gate());
        session();
        if (sessionPlayer && i && isGear(i)) {
            int enemy = playerLevel();
            if (source &&
                source->entity_data.hostility_type == cube::Creature::EntityBehaviour::Hostile)
                enemy = GetCreatureLevel(source);
            registerGear(i, progression::lootLevel(playerLevel(), enemy, itemKey(i)));
        }
    }
    originalDrop(w, i, pos, a, b, amount, source, owner);
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
    registerGear(i, progression::lootLevel(playerLevel(), playerLevel(), k));
    return gear()[k];
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
    IntVector2 r = c->entity_data.current_region;
    if (r == IntVector2(0, 0))
        g->world->GetRegion(&r, blockFromDots(c->entity_data.position.x),
                            blockFromDots(c->entity_data.position.y));
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
    // Trampoline: the 14 original bytes, then jmp [rip+0] back to site + 14.
    auto trampoline = (unsigned char *)VirtualAlloc(nullptr, 64, MEM_RESERVE | MEM_COMMIT,
                                                    PAGE_EXECUTE_READWRITE);
    if (!trampoline)
        return false;
    memcpy(trampoline, site, 14);
    unsigned char j[14] = {0xff, 0x25, 0, 0, 0, 0};
    auto resume = site + 14;
    memcpy(j + 6, &resume, 8);
    memcpy(trampoline + 14, j, 14);
    originalDrop = (DropFn)trampoline;
    DWORD prior;
    if (!VirtualProtect(site, 14, PAGE_EXECUTE_READWRITE, &prior))
        return false;
    // Overwrite the prologue with jmp [rip+0] to drop().
    auto target = &drop;
    memcpy(j + 6, &target, 8);
    memcpy(site, j, 14);
    VirtualProtect(site, 14, prior, &prior);
    FlushInstructionCache(GetCurrentProcess(), site, 14);
    return true;
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
