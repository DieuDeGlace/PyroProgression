#include "src/NativeLootAdapter.h"
#include "src/utility.h"
#include <cassert>
#include <cstdio>
#include <cstdlib>
#include <cstring>

// Standalone test: no SDK allocator or game-address wrapper can be invoked.
void *CWOffset(size_t) {
    std::abort();
}
cube::Item::Item() {}
extern "C" void PyroCommitDropRarity(cube::Item *) noexcept;

void verifyOnlyRarity(const cube::Item &item, int rarity) {
    unsigned char expected[sizeof(item)];
    std::memset(expected, 0x5a, sizeof(expected));
    expected[0] = 3;
    expected[0x14] = static_cast<unsigned char>(rarity);
    assert(std::memcmp(&item, expected, sizeof(item)) == 0);
}

DWORD WINAPI threadCheck(void *) {
    cube::Item item;
    std::memset(&item, 0x5a, sizeof(item));
    item.category = 3;
    item.rarity = 4;
    PyroCommitDropRarity(&item);
    verifyOnlyRarity(item, 4);
    progression::FinalDropRarityScope scope(0);
    PyroCommitDropRarity(&item);
    verifyOnlyRarity(item, 0);
    return 0;
}

int main() {
    cube::Item item;
    std::memset(&item, 0x5a, sizeof(item));
    item.category = 3;
    item.rarity = 4;
    PyroCommitDropRarity(&item);
    verifyOnlyRarity(item, 4);
    {
        progression::FinalDropRarityScope outer(2);
        PyroCommitDropRarity(&item);
        verifyOnlyRarity(item, 2);
        {
            progression::FinalDropRarityScope inner(0);
            PyroCommitDropRarity(&item);
            verifyOnlyRarity(item, 0);
        }
        PyroCommitDropRarity(&item);
        verifyOnlyRarity(item, 2);
        {
            progression::FinalDropRarityScope disabled(-1);
            item.rarity = 4;
            PyroCommitDropRarity(&item);
            verifyOnlyRarity(item, 4);
        }
        auto thread = CreateThread(nullptr, 0, threadCheck, nullptr, 0, nullptr);
        assert(thread);
        assert(WaitForSingleObject(thread, 10000) == WAIT_OBJECT_0);
        CloseHandle(thread);
        PyroCommitDropRarity(&item);
        verifyOnlyRarity(item, 2);
        item.category = 24;
        item.rarity = 1;
        unsigned char before[sizeof(item)];
        std::memcpy(before, &item, sizeof(item));
        PyroCommitDropRarity(&item);
        assert(std::memcmp(before, &item, sizeof(item)) == 0);
    }
    item.category = 3;
    item.rarity = 4;
    PyroCommitDropRarity(&item);
    verifyOnlyRarity(item, 4);
    puts(
        "Final loot rarity commit: native rarity replaced, all other Item bytes preserved, nested scope and thread isolation PASS");
}
