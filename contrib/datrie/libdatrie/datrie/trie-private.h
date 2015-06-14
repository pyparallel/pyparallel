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
 * trie-private.h - Private utilities for trie implementation
 * Created: 2007-08-25
 * Author:  Theppitak Karoonboonyanan <thep@linux.thai.net>
 */

#ifndef __TRIE_PRIVATE_H
#define __TRIE_PRIVATE_H

#include <datrie/typedefs.h>

/**
 * @file trie-private.h
 * @brief Private utilities for trie implementation
 */

/**
 * @brief Minimum value macro
 */
#define MIN_VAL(a,b)  ((a)<(b)?(a):(b))
/**
 * @brief Maximum value macro
 */
#define MAX_VAL(a,b)  ((a)>(b)?(a):(b))

#endif  /* __TRIE_PRIVATE_H */

/*
vi:ts=4:ai:expandtab
*/
