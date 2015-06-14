/* -*- Mode: C; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4 -*- */
/*
 * libdatrie - Double-Array Trie Library
 * Copyright (C) 2006  Theppitak Karoonboonyanan <thep@linux.thai.net>
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public
 * License as published by the Free Software Foundation; either
 * version 2.1 of the License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public
 * License along with this library; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA  02110-1301  USA
 */

/*
 * fileutils.h - File utility functions
 * Created: 2006-08-15
 * Author:  Theppitak Karoonboonyanan <thep@linux.thai.net>
 */

#include <string.h>
#include <stdlib.h>

#include "fileutils.h"

/* ==================== BEGIN IMPLEMENTATION PART ====================  */

/*--------------------------------*
 *    FUNCTIONS IMPLEMENTATIONS   *
 *--------------------------------*/

Bool
file_read_int32 (FILE *file, int32 *o_val)
{
    unsigned char   buff[4];

    if (fread (buff, 4, 1, file) == 1) {
        *o_val = (buff[0] << 24) | (buff[1] << 16) |  (buff[2] << 8) | buff[3];
        return TRUE;
    }

    return FALSE;
}

Bool
file_write_int32 (FILE *file, int32 val)
{
    unsigned char   buff[4];

    buff[0] = (val >> 24) & 0xff;
    buff[1] = (val >> 16) & 0xff;
    buff[2] = (val >> 8) & 0xff;
    buff[3] = val & 0xff;

    return (fwrite (buff, 4, 1, file) == 1);
}

Bool
file_read_int16 (FILE *file, int16 *o_val)
{
    unsigned char   buff[2];

    if (fread (buff, 2, 1, file) == 1) {
        *o_val = (buff[0] << 8) | buff[1];
        return TRUE;
    }

    return FALSE;
}

Bool
file_write_int16 (FILE *file, int16 val)
{
    unsigned char   buff[2];

    buff[0] = val >> 8;
    buff[1] = val & 0xff;

    return (fwrite (buff, 2, 1, file) == 1);
}

Bool
file_read_int8 (FILE *file, int8 *o_val)
{
    return (fread (o_val, sizeof (int8), 1, file) == 1);
}

Bool
file_write_int8 (FILE *file, int8 val)
{
    return (fwrite (&val, sizeof (int8), 1, file) == 1);
}

Bool
file_read_chars (FILE *file, char *buff, int len)
{
    return (fread (buff, sizeof (char), len, file) == len);
}

Bool
file_write_chars (FILE *file, const char *buff, int len)
{
    return (fwrite (buff, sizeof (char), len, file) == len);
}

/*
vi:ts=4:ai:expandtab
*/
