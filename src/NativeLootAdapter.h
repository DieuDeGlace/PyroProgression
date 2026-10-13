#pragma once

namespace progression {

// Native item rarities are 0..4. A negative value leaves the native roll alone.
// The scope surrounds originalDrop so only that new drop receives its chosen
// final rarity, including when native enemy loot applies additional upgrades.
class FinalDropRarityScope {
  public:
    explicit FinalDropRarityScope(int nativeRarity) noexcept;
    ~FinalDropRarityScope() noexcept;
    FinalDropRarityScope(const FinalDropRarityScope &) = delete;
    FinalDropRarityScope &operator=(const FinalDropRarityScope &) = delete;

  private:
    int previous_;
};

// Guarded Steam-build adapter, installed during normal mod initialization.
bool SetupNativeFinalLoot();

} // namespace progression
