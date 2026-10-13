PyroProgression v1.4 — relative progression variants

PlayerRelative: enemy numeric levels follow the player's current level. Installed by default.
DiscoveryLocked: the first entry into a native region records the player's level as that region's permanent baseline. Applies to regions, including their named locations. Enemy species and star rarity remain unchanged. This copy is saved outside Mods; use only one progression DLL at a time.

Easy: plains, hills, woodlands, ocean, wetlands, mountains. Roughly 90–98% of player level, with ±1 creature variation and a minimum level of 1.
Moderate: dark woods, snow forests, savannah, snowlands. Roughly 108–116%, minimum player level +1.
Hard: jungle, desert, deadlands/undead lands, fire/lava lands. Roughly 125–133%, minimum player level +2.
Region rolls use world seed and region coordinates, so they remain consistent across restarts.

Unregistered equipped/inventory gear gets player level minus 0–2 (minimum 1), then keeps that level. Existing saved gear levels stay fixed. A saved level is repaired only when the owned item's native region is (0,0), it exactly matches the old distance calculation, and it exceeds the maximum relative loot level for the player; this corrects starter inventory levels caused by distant spawn regions. Native drops from a hostile enemy above player level in a moderate or hard biome have a one-in-three chance to get a level between player+1 and enemy level. New drops from easy biomes or without a hostile source stay at or below player level. Higher-level gear retains the original equip restriction: reach its level before wearing it. Star rarity, modifiers and spirits are not rewritten.
Item levels are identified by character/world, category, item ID, original modifier and material. Identical native item seeds share a level. Region and rarity upgrades keep the item level.

State persists in Mods/PyroRelativeLevels.state. Back up this file with your character saves. Discovery starts on first entry with this variant; old exploration history cannot be reconstructed. Region baseline records are per character/world. New gear still follows current player/killed enemy level in the locked variant.

The v1.4 XP requirement curve and player/enemy stat formulas are retained. New relative enemy levels feed the existing formulas. The higher-level XP bonus is capped at a 20-level difference to avoid exponential overflow. /recenter no longer moves the progression origin or heals creatures. Existing distance-based prices for non-gear and gold bag values remain as before.

Installation: replace Mods/PyroProgression_v.1.4.dll with the DLL from the chosen package; restart Steam Cube World. Never install both copies together. Other mods are unchanged. Loader API 7.3 is retained.

Verification: native biome and drop signatures read from the running Steam executable; pure biome/loot rules passed; audit-only DLL load and live gear preservation passed. Actual hooked gameplay requires a restart and further testing.
