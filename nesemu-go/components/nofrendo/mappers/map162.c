/*
** map162.c
** Mapper 162 and 163 interface
** by ducalex
*/

#include <noftypes.h>
#include <nofrendo.h>
#include <nes_mmc.h>
#include <nes_ppu.h>
#include <nes.h>

static uint8 reg5000;
static uint8 reg5100;
static uint8 reg5101;
static uint8 reg5200;
static uint8 reg5300;
static uint8 trigger;

static void sync()
{
   uint8 bank = (reg5200 & 0x3) << 4 | (reg5000 & 0xF);
   mmc_bankrom(32, 0x8000, bank);
   // mmc_bankvrom(8, 0x0000, 0);
}

static void map162_init(void)
{
   reg5000 = 0;
   reg5100 = 1;
   reg5101 = 1;
   reg5200 = 0;
   reg5300 = 0;
   trigger = 0;
   mmc_bankvrom(4, 0x0000, 0);
   mmc_bankvrom(4, 0x1000, 0);
   sync();

   //
   // mmc_getinfo()->vrom_banks = 0xFF;
}

static void map162_hblank(int scanline)
{
   if ((reg5000 & 0x80))
   {
      if (scanline == 127)
      {
         mmc_bankvrom(4, 0x0000, 1);
         mmc_bankvrom(4, 0x1000, 1);
      }
      else if (scanline == 239)
      {
         mmc_bankvrom(4, 0x0000, 0);
         mmc_bankvrom(4, 0x1000, 0);
      }
   }
}

static uint8 map162_reg_read(uint32 address)
{
   switch (address & 0x7300)
   {
   case 0x5100:
      return reg5300; // | reg5200 | reg5000 | (reg5100 ^ 0xFF);

   case 0x5500:
      return trigger ? reg5300 : 0;

   default:
      MESSAGE_ERROR("mapper 162: untrapped read to $%04X\n", address);
      return 0x04;
   }
}

static void map162_reg_write(uint32 address, uint8 value)
{
   switch (address & 0x7300)
   {
   case 0x5000:
      reg5000 = value;
      if (!(reg5000 & 0x80) && nes_getptr()->scanline < 128)
      {
         mmc_bankvrom(8, 0x0000, 0);
      }
      sync();
      break;

   case 0x5100:
      if (address == 0x5101) // Copy protection
      {
         if (reg5101 && !value)
            trigger = !trigger;
         reg5101 = value;
      }
      else if (value == 0x6)
      {
         mmc_bankrom(32, 0x8000, 3);
      }
      reg5100 = value;
      break;

   case 0x5200:
      reg5200 = value;
      sync();
      break;

   case 0x5300:
      reg5300 = value;
      break;

   default:
      MESSAGE_ERROR("mapper 162: untrapped write $%02X to $%04X\n", value, address);
   }
}

static map_memwrite map162_memwrite[] =
{
   {0x5000, 0x5FFF, map162_reg_write},
   // {0x6000, 0x7FFF, map162_ram_write},
   {-1, -1, NULL}
};

static map_memwrite map162_memread[] =
{
   {0x5000, 0x5FFF, map162_reg_read},
   // {0x6000, 0x7FFF, map162_ram_read},
   {-1, -1, NULL}
};

mapintf_t map162_intf =
{
   162,             /* mapper number */
   "Nanjing 162",   /* mapper name */
   map162_init,     /* init routine */
   NULL,            /* vblank callback */
   map162_hblank,   /* hblank callback */
   NULL,            /* get state (snss) */
   NULL,            /* set state (snss) */
   map162_memread,  /* memory read structure */
   map162_memwrite, /* memory write structure */
   NULL             /* external sound device */
};

mapintf_t map163_intf =
{
   163,             /* mapper number */
   "Nanjing 163",   /* mapper name */
   map162_init,     /* init routine */
   NULL,            /* vblank callback */
   map162_hblank,   /* hblank callback */
   NULL,            /* get state (snss) */
   NULL,            /* set state (snss) */
   map162_memread,  /* memory read structure */
   map162_memwrite, /* memory write structure */
   NULL             /* external sound device */
};
