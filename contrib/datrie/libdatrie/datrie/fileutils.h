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
 * Created: 2006-08-14
 * Author:  Theppitak Karoonboonyanan <thep@linux.thai.net>
 */

#ifndef __FILEUTILS_H
#define __FILEUTILS_H

#include <stdio.h>
#include <datrie/typedefs.h>

Bool   file_read_int32 (FILE *file, int32 *o_val);
Bool   file_write_int32 (FILE *file, int32 val);

Bool   file_read_int16 (FILE *file, int16 *o_val);
Bool   file_write_int16 (FILE *file, int16 val);

Bool   file_read_int8 (FILE *file, int8 *o_val);
Bool   file_write_int8 (FILE *file, int8 val);

Bool   file_read_chars (FILE *file, char *buff, int len);
Bool   file_write_chars (FILE *file, const char *buff, int len);

#endif /* __FILEUTILS_H */

/*
vi:ts=4:ai:expandtab
*/
