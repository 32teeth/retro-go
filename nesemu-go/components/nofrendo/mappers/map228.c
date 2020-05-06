/*
** map228.c
** Mapper 228 interface
** by ducalex
*/

#include <noftypes.h>
#include <nes_mmc.h>
#include <nes.h>

static uint8 mram[4];

static void update(uint32 address, uint8 value)
{
	uint16 bank1 = ((address >> 6) & 0x7E) + (((address >> 6) & 1) & ((address >> 5) & 1));
    uint16 bank2 = bank1 + (((address >> 5) & 1) ^ 1);
    uint16 vbank = (((address & 0xF) << 2)) | (value & 0x3);

    // Chip 2 doesn't exist
	if ((bank1 & 0x60) == 0x60) {
		bank1 -= 0x20;
		bank2 -= 0x20;
    }

    mmc_bankrom(16, 0x8000, bank1);
    mmc_bankrom(16, 0xc000, bank2);
    mmc_bankvrom(8, 0x0000, vbank);

    if ((address & 0x2000) == 0) {
        ppu_mirror(0, 1, 0, 1); /* vertical */
    } else {
        ppu_mirror(0, 0, 1, 1); /* horizontal */
    }
}

static void map228_init(void)
{
    update(0x8000, 0x00);
}

static uint8 map228_read(uint32 address)
{
    return mram[address & 3] & 0xF;
}

static void map228_write(uint32 address, uint8 value)
{
    if (address >= 0x4020 && address <= 0x5FFF)
    {
        mram[address & 3] = value;
    }
    else
    {
        update(address, value);
    }
}

static map_memwrite map228_memwrite [] =
{
    { 0x8000, 0xFFFF, map228_write },
    { 0x4020, 0x5FFF, map228_write },
    {     -1,     -1, NULL }
};

static map_memread map228_memread [] =
{
    { 0x4020, 0x5FFF, map228_read },
    {     -1,     -1, NULL }
};

mapintf_t map228_intf =
{
    228,                              /* Mapper number */
    "Mapper 228",                     /* Mapper name */
    map228_init,                      /* Initialization routine */
    NULL,                             /* VBlank callback */
    NULL,                             /* HBlank callback */
    NULL,                             /* Get state (SNSS) */
    NULL,                             /* Set state (SNSS) */
    map228_memread,                   /* Memory read structure */
    map228_memwrite,                  /* Memory write structure */
    NULL                              /* External sound device */
};
