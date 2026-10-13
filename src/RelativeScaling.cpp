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
#define gate data().gate
#define gear data().gear
#define regions data().regions
#define biomes data().biomes
cube::Creature *sessionPlayer = nullptr;
cube::World *sessionWorld = nullptr;
uint64_t identity = 0;
bool loaded = false, locked = DISCOVERY_LOCK_DEFAULT != 0;
const char *statePath = "Mods/PyroRelativeLevels.state";
int playerLevel() {
    auto g = cube::GetGame();
    auto p = g && g->world ? g->GetPlayer() : nullptr;
    return p ? std::max(1, int(p->entity_data.level)) : 1;
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
void save(char kind, uint64_t key, int level) {
    std::ofstream out(statePath, std::ios::app);
    out << kind << ' ' << key << ' ' << level << '\n';
}
void registerGear(cube::Item *i, int level) {
    if (!i || i->category < 3 || i->category > 9)
        return;
    auto k = itemKey(i);
    if (gear.emplace(k, std::max(1, level)).second)
        save('G', k, std::max(1, level));
}
int legacy(cube::Item *i) {
    return 1 + LEVELS_PER_REGION * GetRegionDistance(i->region) +
           GetLevelVariation(i->modifier, LEVELS_PER_REGION);
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
        while (in >> kind >> key >> l)
            if (l > 0 && l <= 10000000) {
                if (kind == 'G')
                    gear[key] = l;
                else if (kind == 'R')
                    regions[key] = l;
            }
        loaded = true;
    }
    if (p == sessionPlayer && g->world == sessionWorld)
        return;
    sessionPlayer = p;
    sessionWorld = g->world;
    biomes.clear();
    identity = progression::mix(uint32_t(g->world->seed));
    for (unsigned char c : g->world->world_name)
        identity = progression::mix(identity ^ c);
    for (int n = 0; n < 16 && p->entity_data.name[n]; n++)
        identity = progression::mix(identity ^ uint8_t(p->entity_data.name[n]));
    locked = DISCOVERY_LOCK_DEFAULT != 0 ||
             GetPrivateProfileIntW(L"PyroProgression", L"LockDiscoveredRegions", 0,
                                   L".\\Mods\\PyroProgression.ini") != 0;
    int player = playerLevel();
    auto &e = p->entity_data.equipment;
    for (auto i : {&e.neck, &e.chest, &e.feet, &e.hands, &e.shoulder, &e.weapon_left,
                   &e.weapon_right, &e.ring_left, &e.ring_right})
        registerOwnedGear(i, player);
    for (auto &tab : p->inventory_tabs)
        for (auto &stack : tab)
            registerOwnedGear(&stack.item, player);
}
int biome(IntVector2 r) {
    auto key = progression::regionRoll(0, r.x, r.y);
    auto it = biomes.find(key);
    if (it != biomes.end())
        return it->second;
    auto g = cube::GetGame();
    int b = ((int (*)(cube::World *, int, int))CWOffset(0x2b9080))(g->world, r.x, r.y);
    biomes[key] = b;
    return b;
}
int basis(IntVector2 r) {
    auto it = regions.find(regionKey(r));
    return locked && it != regions.end() ? it->second : playerLevel();
}
using DropFn = void (*)(cube::World *, cube::Item *, LongVector3 *, float, float, int,
                        cube::Creature *, uint64_t);
DropFn originalDrop = nullptr;
void drop(cube::World *w, cube::Item *i, LongVector3 *pos, float a, float b, int amount,
          cube::Creature *source, uint64_t owner) {
    {
        std::lock_guard<std::recursive_mutex> guard(gate);
        session();
        if (sessionPlayer && i && i->category >= 3 && i->category <= 9) {
            int player = playerLevel();
            int enemy = player;
            int sourceBiome = 0;
            if (source &&
                source->entity_data.hostility_type == cube::Creature::EntityBehaviour::Hostile) {
                enemy = GetCreatureLevel(source);
                sourceBiome = biome(creatureRegion(source));
            }
            registerGear(i, progression::lootLevel(player, enemy, sourceBiome, itemKey(i)));
        }
    }
    originalDrop(w, i, pos, a, b, amount, source, owner);
}
} // namespace
int RelativeItemLevel(cube::Item *i) {
    std::lock_guard<std::recursive_mutex> guard(gate);
    session();
    if (!i)
        return 1;
    if (!sessionPlayer)
        return 1;
    if (i->category < 3 || i->category > 9)
        return legacy(i);
    auto k = itemKey(i);
    auto it = gear.find(k);
    if (it != gear.end())
        return it->second;
    int player = playerLevel();
    registerGear(i, progression::lootLevel(player, player, 0, k));
    return gear[k];
}
int RelativeCreatureLevel(cube::Creature *c) {
    std::lock_guard<std::recursive_mutex> guard(gate);
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
void RelativeTick(cube::Game *g) {
    std::lock_guard<std::recursive_mutex> guard(gate);
    session();
    if (!sessionPlayer || !g || !g->world)
        return;
    if (locked) {
        IntVector2 r;
        auto p = sessionPlayer->entity_data.position;
        auto block = [](int64_t n) { return int(n / 65536 - (n < 0 && n % 65536 != 0)); };
        g->world->GetRegion(&r, block(p.x), block(p.y));
        auto key = regionKey(r);
        if (regions.emplace(key, playerLevel()).second)
            save('R', key, playerLevel());
    }
}
bool SetupRelativeLoot() {
    auto site = (unsigned char *)CWOffset(0x2a5b70);
    const unsigned char expected[14] = {0x48, 0x8b, 0xc4, 0x55, 0x56, 0x57, 0x41,
                                        0x54, 0x41, 0x55, 0x41, 0x56, 0x41, 0x57};
    if (memcmp(site, expected, 14) != 0)
        return false;
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
    auto target = &drop;
    memcpy(j + 6, &target, 8);
    memcpy(site, j, 14);
    VirtualProtect(site, 14, prior, &prior);
    FlushInstructionCache(GetCurrentProcess(), site, 14);
    return true;
}
extern "C" void ModPreInitialize();
EXPORT DWORD WINAPI AuditRelativeProgression(void *) {
    ModPreInitialize();
    std::lock_guard<std::recursive_mutex> guard(gate);
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
