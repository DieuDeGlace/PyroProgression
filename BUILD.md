# PyroProgression fork: player-relative scaling

Edited with AI assistance (OpenAI ChatGPT/Codex). Based on upstream [PyroProgression](https://github.com/gijsgroenewegen/PyroProgression), tag `v.1.4`, and its original CWSDK revision `7301c4635ed9778fe8db29793aa0981354ef6447`. Original upstream code remains attributed to its authors; no new blanket license is asserted for upstream files without a license notice.

`main` defaults to player-relative enemy levels. `discovery-locked` defaults to permanent region baselines recorded on first entry, per character/world. Both include biome difficulty rolls, stable existing gear levels and above-player gear chances from higher-level enemies. See [PROGRESSION.md](PROGRESSION.md) for exact behavior and limitations. Native star rarity stays separate.

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

State is stored in `Mods/PyroRelativeLevels.state`; keep it with your save backups. Discovery begins when this variant is installed, not from historical map exploration. New loot in already locked regions still uses current player/killed enemy levels.

Validation: both variants compiled locally; biome/loot rules passed; the player-relative DLL was loaded by the game, native drop hook activation was confirmed and owned gear levels were checked. These checks do not establish balance across every biome or long-session stability.
