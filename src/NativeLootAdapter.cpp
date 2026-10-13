#include "NativeLootAdapter.h"
#include "utility.h"
#include <cstddef>
#include <cstring>
#include <windows.h>

namespace {
thread_local int desiredRarity = -1;
bool installed = false;
} // namespace

static_assert(offsetof(cube::Item, rarity) == 0x14, "Native drop rarity layout changed");

extern "C" {
__attribute__((used)) void *PyroFinalDropResume = nullptr;

void PyroCommitDropRarity(cube::Item *item) noexcept {
    if (desiredRarity >= 0 && desiredRarity <= 4 && item->category >= 3 && item->category <= 9) {
        item->rarity = static_cast<char>(desiredRarity);
    }
}

// Steam RVA 2a669f is after all native rarity upgrades and immediately before
// local Item.rarity [rbp-5c] is copied into GroundItem.rarity [rbp+44].
// The game's frame has aligned RSP here. Preserve flags and every volatile
// Windows-x64 register, supply 32 bytes of shadow space, then replay all five
// displaced instructions. Callee-saved GPRs and XMM6..15 follow the native ABI.
__attribute__((naked)) void PyroFinalDropThunk() {
    asm(".intel_syntax\n"
        "pushfq\n"
        "push rax\n"
        "push rcx\n"
        "push rdx\n"
        "push r8\n"
        "push r9\n"
        "push r10\n"
        "push r11\n"
        "sub rsp, 0x80\n"
        "movups xmmword ptr [rsp+0x20], xmm0\n"
        "movups xmmword ptr [rsp+0x30], xmm1\n"
        "movups xmmword ptr [rsp+0x40], xmm2\n"
        "movups xmmword ptr [rsp+0x50], xmm3\n"
        "movups xmmword ptr [rsp+0x60], xmm4\n"
        "movups xmmword ptr [rsp+0x70], xmm5\n"
        "lea rcx, [rbp-0x70]\n"
        "call PyroCommitDropRarity\n"
        "movups xmm0, xmmword ptr [rsp+0x20]\n"
        "movups xmm1, xmmword ptr [rsp+0x30]\n"
        "movups xmm2, xmmword ptr [rsp+0x40]\n"
        "movups xmm3, xmmword ptr [rsp+0x50]\n"
        "movups xmm4, xmmword ptr [rsp+0x60]\n"
        "movups xmm5, xmmword ptr [rsp+0x70]\n"
        "add rsp, 0x80\n"
        "pop r11\n"
        "pop r10\n"
        "pop r9\n"
        "pop r8\n"
        "pop rdx\n"
        "pop rcx\n"
        "pop rax\n"
        "popfq\n"
        "movzx eax, byte ptr [rbp-0x5c]\n"
        "mov byte ptr [rbp+0x44], al\n"
        "mov eax, dword ptr [rbp-0x58]\n"
        "mov dword ptr [rbp+0x48], eax\n"
        "movzx eax, byte ptr [rbp-0x54]\n"
        "jmp qword ptr [rip+PyroFinalDropResume]\n");
}
}

namespace progression {
FinalDropRarityScope::FinalDropRarityScope(int nativeRarity) noexcept : previous_(desiredRarity) {
    desiredRarity = nativeRarity >= 0 && nativeRarity <= 4 ? nativeRarity : -1;
}

FinalDropRarityScope::~FinalDropRarityScope() noexcept {
    desiredRarity = previous_;
}

bool SetupNativeFinalLoot() {
    if (installed)
        return true;
    auto site = reinterpret_cast<unsigned char *>(CWOffset(0x2a669f));
    const unsigned char expected[17] = {0x0f, 0xb6, 0x45, 0xa4, 0x88, 0x45, 0x44, 0x8b, 0x45,
                                        0xa8, 0x89, 0x45, 0x48, 0x0f, 0xb6, 0x45, 0xac};
    if (std::memcmp(site, expected, sizeof(expected)) != 0)
        return false;

    unsigned char jump[sizeof(expected)] = {0xff, 0x25, 0, 0, 0, 0};
    auto target = &PyroFinalDropThunk;
    static_assert(sizeof(target) == 8, "Native loot adapter requires Windows x64");
    std::memcpy(jump + 6, &target, sizeof(target));
    jump[14] = jump[15] = jump[16] = 0x90;
    PyroFinalDropResume = site + sizeof(expected);
    DWORD prior;
    if (!VirtualProtect(site, sizeof(expected), PAGE_EXECUTE_READWRITE, &prior))
        return false;
    std::memcpy(site, jump, sizeof(jump));
    DWORD ignored;
    VirtualProtect(site, sizeof(expected), prior, &ignored);
    FlushInstructionCache(GetCurrentProcess(), site, sizeof(expected));
    installed = true;
    return true;
}
} // namespace progression
