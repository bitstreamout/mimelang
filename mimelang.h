/*
 * mimelang.h
 *
 * Copyright 2021 Werner Fink, 2021 SUSE Software Solutions Germany GmbH.
 *
 * This source is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 */

typedef struct unicode_block {
	const wchar_t start;
	const wchar_t end;
	const char* description;
} unicode_block_t;

extern const unicode_block_t blocks[];
extern const size_t codepoints;
