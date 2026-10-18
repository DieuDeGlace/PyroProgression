#pragma once

// Class armor: each standard class wears its own base armor material (as the
// native class loot is split): warriors plate (iron, steel, bronze), rangers
// linen, mages silk and rogues cotton. A class may not wear another class's base
// armor material. Other materials (special or unknown ones) are never blocked
// here, so rare armor keeps whatever the game allows. CubeMega's Monk (class 5)
// wears cotton like a rogue.
namespace classarmor {
enum Material { Iron = 1, Steel = 4, Silk = 26, Linen = 27, Cotton = 28, Bronze = 29 };
enum Category { Chest = 4, Gloves = 5, Boots = 6, Shoulder = 7 };

// The class (1 warrior, 2 ranger, 3 mage, 4 rogue) whose base armor this
// material is, or 0 for any other material.
inline int baseArmorClass(int material) {
    switch (material) {
    case Iron:
    case Steel:
    case Bronze:
        return 1;
    case Linen:
        return 2;
    case Silk:
        return 3;
    case Cotton:
        return 4;
    default:
        return 0;
    }
}
// False when `classType` must not wear this armor piece.
inline bool allowed(int category, int material, int classType) {
    if (category < Chest || category > Shoulder)
        return true;
    int wearer = classType == 5 ? 4 : classType;
    if (wearer < 1 || wearer > 4)
        return true;
    int owner = baseArmorClass(material);
    return owner == 0 || owner == wearer;
}
} // namespace classarmor
