/*
 * Copyright 2013, Alexander von Gluck, All Rights Reserved.
 * Distributed under the terms of the MIT License.
 *
 * Authors:
 *		Alexander von Gluck IV <kallisti5@unixzen.com>
 */
#ifndef _ROMDB_H
#define _ROMDB_H


#include <stdio.h>

#include "cleantypes.h"


// Rom Flags
#define TWO_PART_ROM 0x0001
#define CD_SYSTEM    0x0002

#define US_ENCODED   0x0010
#define POPULOUS     0x0020

#define USA          0x4000
#define JAP          0x8000

// Known Rom Count (from kKnownRoms)
#define KNOWN_ROM_COUNT 421

struct rom_database {
	const uint32    CRC;
	const char		*Name;
	const char		*Publisher;
	const char		*ID;
	const char		*Date;
	const uint32    Flags;
};

extern const struct rom_database kKnownRoms[KNOWN_ROM_COUNT];


unsigned long filesize(FILE * F);
uint32 CRC_buffer(uchar *buffer, int size);

#endif /* _ROMDB_H */
