#include "src/ClassArmorRules.h"
#include <cassert>
#include <cstdio>
#include <initializer_list>
using namespace classarmor;
int main() {
    // A rogue cannot wear iron shoulders (or any other class's base armor).
    assert(!allowed(Shoulder, Iron, 4));
    for (int material : {Iron, Steel, Bronze, Linen, Silk})
        for (int category = Chest; category <= Shoulder; ++category)
            assert(!allowed(category, material, 4));
    // Each class wears its own base armor.
    for (int category = Chest; category <= Shoulder; ++category) {
        assert(allowed(category, Iron, 1) && allowed(category, Steel, 1) &&
               allowed(category, Bronze, 1));
        assert(allowed(category, Linen, 2) && allowed(category, Silk, 3) &&
               allowed(category, Cotton, 4));
        assert(!allowed(category, Cotton, 1) && !allowed(category, Silk, 2) &&
               !allowed(category, Linen, 3));
    }
    // Monk (CubeMega class 5) follows the rogue's cotton.
    assert(allowed(Chest, Cotton, 5) && !allowed(Chest, Iron, 5));
    // Weapons, jewelry and unlisted (special) materials are left to the game.
    assert(allowed(3, Iron, 3) && allowed(8, Iron, 4) && allowed(9, 12, 1));
    assert(allowed(Chest, 40, 4) && allowed(Shoulder, 0, 2));
    // Unknown classes are not restricted here.
    assert(allowed(Shoulder, Iron, 0) && allowed(Shoulder, Iron, 9));
    puts("Class armor rules: PASS");
}
