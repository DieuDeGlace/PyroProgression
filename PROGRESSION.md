PyroProgression v1.4 — relative progression variants

PlayerRelative (`main`): enemy numeric levels follow the player's current level. Installed by default.
DiscoveryLocked (`discovery-locked`): the first entry into a native region records the player's level as that region's permanent baseline. This includes its named locations. Enemy species and enemy star rarity remain unchanged in both variants. Use only one progression DLL at a time.

Enemy levels use the current player level in PlayerRelative and the recorded region baseline in DiscoveryLocked:

| Difficulty | Biomes | Numeric enemy level |
| --- | --- | --- |
| Easy | Plains, hills, woodlands, ocean, wetlands, mountains | Roughly 90–98% of the baseline, with ±1 creature variation; minimum 1 |
| Moderate | Dark woods, snow forests, savannah, snowlands (native biomes 5, 6, 10, 11) | Roughly 108–116%, with ±1 variation; minimum baseline +1 |
| Hard | Jungle, desert, deadlands/undead lands, fire/lava lands (native biomes 4, 9, 12, 13) | Roughly 125–133%, with ±1 variation; minimum baseline +2 |

Region rolls use the world seed and region coordinates and remain consistent across restarts. Discovery starts on first entry with the locked variant; old exploration history cannot be reconstructed. Region baselines are per character/world.

In both variants, new gear's numeric level is at least the player's current level, including gear from previously locked regions. Most new gear matches that level. Above-player chances are 10%/20%/30% outdoors in easy/moderate/hard biomes and 15%/25%/35% in dungeons. If a hostile source is above the player, a higher drop ranges from player +1 to the lower of its actual numeric level and the relative loot ceiling. Other rewards, including chests and drops without an above-player source, can be up to 1, 2 or 3 levels above the player in easy, moderate or hard biomes respectively. The relative ceiling is the larger of player +3 and the strongest hard-biome enemy level. Higher-level gear retains the original equip restriction: reach its level before wearing it.

New gear star rarity follows these percentage weights. Every biome and reward context can produce all five star tiers; easy, moderate and hard each favor 3, 4 and 5 stars respectively. Dungeons increase both 4-star and 5-star chances.

| Difficulty | Reward context | 1 star | 2 stars | 3 stars | 4 stars | 5 stars |
| --- | --- | ---: | ---: | ---: | ---: | ---: |
| Easy | Outdoors | 10% | 20% | 50% | 15% | 5% |
| Easy | Dungeon | 4% | 11% | 35% | 30% | 20% |
| Moderate | Outdoors | 5% | 10% | 25% | 45% | 15% |
| Moderate | Dungeon | 2% | 5% | 12% | 46% | 35% |
| Hard | Outdoors | 3% | 7% | 15% | 30% | 45% |
| Hard | Dungeon | 1% | 4% | 8% | 32% | 55% |

Native item rarity values 0–4 represent 1–5 stars. The adapter commits the selected rarity after native rarity enhancements and immediately before copying the item into the world drop, so the table applies to final drop rarity. The override is scoped to that new drop. Modifiers and spirits keep their native behavior.

Dungeon context is resolved from the hostile source position, or the drop position for chest and source-free rewards, before the world item is created. It requires a finite, positive location footprint and recognizes direct native dungeon types 4, 7, 8, 12, 15 and 40–47. Sewers and catacombs beneath other locations also require an actual underground position and a matching native underground dungeon type. Chest and source-free rewards use the biome at their drop position; hostile-source rewards use the source's biome.

Unregistered equipped/inventory gear gets the player's level once and then keeps it. Confirming creation of a fresh level-1 character forces all starter equipment and inventory gear to level 1, including Monk gear, through the inspected Steam character-creation hook at RVA `0x9ea10`. Existing owned gear keeps its recorded level through player leveling and item upgrades. Automatic legacy repair requires all three markers: the owned item's native region is (0,0), its saved level exactly matches the old distance calculation, and it exceeds the maximum relative loot level. Older distance-based records that no longer retain these markers require a targeted state repair.

Item identity combines character/world, category, item ID, original modifier and material. Region, rarity and spirit changes preserve that identity and the existing item's level. State persists in `Mods/PyroRelativeLevels.state`: `G` records store gear levels, `R` records store discovered region baselines, and `T` records store selected final native star rarity. Keep this file with character save backups.

The v1.4 XP requirement curve and player/enemy stat formulas are retained. Relative enemy levels feed the existing formulas. The higher-level XP bonus is capped at a 20-level difference to avoid exponential overflow. `/recenter` no longer moves the progression origin or heals creatures. Existing distance-based prices for non-gear and gold bag values remain as before.

Installation: replace `Mods/PyroProgression_v.1.4.dll` with the DLL from the chosen package, then restart Steam Cube World. Install exactly one progression variant. Other mods are unchanged. Loader API 7.3 and the inspected Steam executable are required. Proprietary game files and save data are not distributed.

Verification: both variants build locally and each runs two tests: the pure biome/loot rules and the native adapter helper. These cover the specified distributions, numeric level bounds, registration/migration rules and scoped final-rarity handling. Local native signatures and hook sites were inspected, and the earlier audit load checked existing gear preservation. These checks do not establish live drop balance in every biome, dungeon classification throughout gameplay, or long-session stability.
