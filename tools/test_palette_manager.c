/* Exercise the production allocator; no hardware interface is used here. */
#define _MEMORYMANAGER_H_
#include "../src/paletteManager.c"

#define CHECK(c, m) do { if (!(c)) { fprintf(stderr, "%s\n", m); exit(1); } } while (0)

int main(void)
{
    PaletteGetResult result;
    memset(allocatedPaletteOwners, 0xA5, sizeof(allocatedPaletteOwners));
    bFInitPaletteManager();
    for (unsigned i = 0; i < NO_MANAGED_PALETTES; ++i)
        CHECK(allocatedPaletteOwners[i] == 0, "Palette reset left stale owner bytes");
    CHECK(palettesAllocated == 0, "Palette reset retained the allocation count");
    for (unsigned i = 0; i < NO_MANAGED_PALETTES; ++i) {
        CHECK(bFGetPalette(100 + i, &result) == BASE_MANAGED_PALETTE + i && result == Allocated,
              "Palette allocation returned the wrong slot/status");
        CHECK(bFGetPalette(100 + i, &result) == BASE_MANAGED_PALETTE + i && result == AlreadyAllocated,
              "Palette reuse failed at the capacity boundary");
    }
    for (unsigned i = 0; i < NO_MANAGED_PALETTES; ++i)
        CHECK(bFGetPalette(100 + i, &result) == BASE_MANAGED_PALETTE + i && result == AlreadyAllocated,
              "Full palette cache rejected an existing owner");
    CHECK(bFGetPalette(200, &result) == 0 && result == FailToAllocate,
          "Full palette cache accepted a new owner");
    CHECK(palettesAllocated == NO_MANAGED_PALETTES, "Reuse/failure changed the allocation count");
    bFInitPaletteManager();
    CHECK(bFGetPalette(200, &result) == BASE_MANAGED_PALETTE && result == Allocated,
          "Palette cache could not allocate after reset");
    puts("Palette manager checks pass: reset width, all slots, full-cache reuse, failure and restart");
    return 0;
}
