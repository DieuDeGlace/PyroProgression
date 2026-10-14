# PyroProgression fork: discovery-locked scaling

Edited with AI assistance (OpenAI ChatGPT/Codex). Based on upstream [PyroProgression](https://github.com/gijsgroenewegen/PyroProgression), tag `v.1.4`, and its original CWSDK revision `7301c4635ed9778fe8db29793aa0981354ef6447`. Original upstream code remains attributed to its authors; no new blanket license is asserted for upstream files without a license notice.

`main` defaults to enemy levels following the player's current level. `discovery-locked` defaults to permanent region baselines recorded on first entry, per character/world. Both guarantee newly generated gear at the current player's level or above, favor 3/4/5-star gear in easy/moderate/hard biomes, and increase 4-star and 5-star chances in dungeons. Existing owned gear keeps its level; fresh level-1 character creation also fixes all starter gear at level 1, including Monk gear. See [PROGRESSION.md](PROGRESSION.md) for the complete chance tables, level bounds and dungeon detection rules.

## Build on Windows x64

Install Git, CMake, Ninja, and Visual Studio 2022 Build Tools with Desktop development with C++, Windows SDK, and LLVM/Clang tools. Use an **x64 Native Tools Command Prompt for VS 2022**, with `clang-cl`, `cmake`, and `ninja` on PATH.

```bat
git clone --recurse-submodules https://github.com/DieuDeGlace/PyroProgression.git
cd PyroProgression
git submodule update --init --recursive
cmake -S . -B build -G Ninja -DCMAKE_C_COMPILER=clang-cl -DCMAKE_CXX_COMPILER=clang-cl -DCMAKE_BUILD_TYPE=RelWithDebInfo
cmake --build build
ctest --test-dir build --output-on-failure
```

For the other variant, clone separately with `git clone --branch discovery-locked --recurse-submodules https://github.com/DieuDeGlace/PyroProgression.git PyroProgression-locked`, then run the same build commands there. Override either default using `-DDISCOVERY_LOCK_DEFAULT=ON` or `OFF`. Use a fresh build directory when switching variants.

Output: `build/PyroProgression.dll`. Close Steam Cube World, replace the existing progression DLL in `Mods` with this file (you may keep the name `PyroProgression_v.1.4.dll`), then restart. Install exactly one progression variant. Loader API 7.3 and the inspected Steam executable are required. No loader, proprietary game files or save data are included.

State is stored in `Mods/PyroRelativeLevels.state`; keep it with your save backups. Gear levels use `G` records, discovered baselines use `R`, selected final native rarity uses `T`, and completed locations use `C`. In multiplayer, enemy levels and discovery baselines follow the highest-level player in the session (`ScaleToHighestPlayer=0` in `Mods/PyroProgression.ini` restores local-only scaling). In the locked variant, a location whose boss has been killed is no longer held at the region baseline. Discovery begins on first entry with the locked variant; historical map exploration cannot be reconstructed. Newly generated loot in locked regions still uses the current player level and, when applicable, its actual hostile source level. The scoped adapter applies the selected star rarity after native enhancements and before the world item is copied; modifiers and spirits keep their native behavior. Personal state records are not distributed.

Validation: both variants compile locally and each runs two tests, `RelativeRules` and `NativeLootAdapter`. The first checks biome tiers, exact star-weight distributions, dungeon improvements, numeric loot bounds, starter registration and strict legacy migration. The second exercises the scoped final-rarity helper and preservation of unrelated item fields. The inspected Steam build's native hook sites are guarded by signature checks, including confirmed character creation at RVA `0x9ea10`. The earlier audit load checked owned gear preservation. Unit tests and adapter-helper tests do not establish live drop balance, complete dungeon coverage, or long-session stability.
