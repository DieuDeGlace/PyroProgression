#include "utility.h"

int GetRegionDistance(IntVector2 region) {
    cube::Game *game = cube::GetGame();
    if (!game || !game->GetPlayer()) {
        return 0;
    }

    IntVector2 base_region = game->GetPlayer()->entity_data.equipment.unk_item.region;
    if (game->GetPlayer()->entity_data.equipment.unk_item.modifier == 0)
        base_region = game->GetPlayer()->entity_data.current_region;
    int xDiv = std::abs(base_region.x - region.x);
    int yDiv = std::abs(base_region.y - region.y);
    return std::max<int>(xDiv, yDiv);
}

int GetItemLevel(cube::Item *item) {
    return RelativeItemLevel(item);
}

void SetEquipmentRegion(cube::Creature *creature, IntVector2 region) {
    cube::Equipment *equipment = &creature->entity_data.equipment;
    equipment->chest.region = region;
    equipment->hands.region = region;
    equipment->feet.region = region;
    equipment->neck.region = region;
    equipment->pet.region = region;
    equipment->ring_left.region = region;
    equipment->ring_right.region = region;
    equipment->weapon_left.region = region;
    equipment->weapon_right.region = region;
    equipment->shoulder.region = region;
}

int GetCreatureLevel(cube::Creature *creature) {
    return RelativeCreatureLevel(creature);
}

// Can also be done without rands but just as a `modifier % range`
int GetLevelVariation(long long modifier, int range) {
    return PyroRand(modifier) % range;
}

unsigned long long PyroRand(unsigned long long seed) {
    auto n = seed * 1103515245 + 12345;
    return (unsigned long long)(n / 65536) % 32768;
}
