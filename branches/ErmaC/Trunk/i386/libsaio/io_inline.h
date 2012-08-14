/*
 * Copyright (c) 1999-2003 Apple Computer, Inc. All rights reserved.
 *
 * 
 * Portions Copyright (c) 1999-2003 Apple Computer, Inc.  All Rights
 * Reserved.  This file contains Original Code and/or Modifications of
 * Original Code as defined in and that are subject to the Apple Public
 * Source License Version 2.0 (the "License").  You may not use this file
 * except in compliance with the License.  Please obtain a copy of the
 * License at http://www.apple.com/publicsource and read it before using
 * this file.
 * 
 * The Original Code and all software distributed under the License are
 * distributed on an "AS IS" basis, WITHOUT WARRANTY OF ANY KIND, EITHER
 * EXPRESS OR IMPLIED, AND APPLE HEREBY DISCLAIMS ALL SUCH WARRANTIES,
 * INCLUDING WITHOUT LIMITATION, ANY WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE OR NON- INFRINGEMENT.  Please see the
 * License for the specific language governing rights and limitations
 * under the License.
 * 
 *
 * Copyright (c) 1992 NeXT Computer, Inc.
 *
 * Inlines for io space access.
 *
 * HISTORY
 *
 * 20 May 1992 ? at NeXT
 *	Created.
 */

#ifndef __LIBSAIO_IO_INLINE_H
#define __LIBSAIO_IO_INLINE_H

/*
 *############################################################################
 *
 * x86 IN/OUT I/O inline functions.
 *
 * IN :  inb, inw, inl
 *       IN(port)
 *
 * OUT:  outb, outw, outl
 *       OUT(port, data)
 *
 *############################################################################
 */

#define __IN(s, u)                            \
static inline unsigned u                      \
in##s(unsigned short port)                    \
{                                             \
    unsigned u data;                          \
    asm volatile (                            \
    	"in" #s " %1,%0"                      \
		: "=a" (data)                         \
		: "d" (port));                        \
    return (data);                            \
}

#define __OUT(s, u)                           \
static inline void                            \
out##s(unsigned short port, unsigned u data)  \
{                                             \
    asm volatile (                            \
    	"out" #s " %1,%0"                     \
		:                                     \
		: "d" (port), "a" (data));            \
}

__IN(b, char)    /* inb() */
__IN(w, short)   /* inw() */
__IN(l, long)    /* inl() */

__OUT(b, char)   /* outb() */
__OUT(w, short)  /* outw() */
__OUT(l, long)   /* outl() */


static inline void cmos_write_byte (int loc, int val)
{
	outb (0x70, loc);
	outb (0x71, val);
}

static inline unsigned cmos_read_byte (int loc)
{
	outb (0x70, loc);
	return inb (0x71);
}

#define CMOS_WRITE_BYTE(x, y)	cmos_write_byte(x, y)
#define CMOS_READ_BYTE(x)	cmos_read_byte(x)

static inline void cli() {
	asm("cli");
}
static inline void sti() {
	asm("sti");
}

#endif /* !__LIBSAIO_IO_INLINE_H */
