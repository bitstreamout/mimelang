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

enum bits {
	MIME_UTF8	= 0001,   /* UTF-8 high bit multi byte characters */
	MIME_LATIN	= 0002,
	MIME_SYMBOL	= 0004,
	MIME_NONEU	= 0010
};

typedef struct unicode_block {
	const wchar_t start;
	const wchar_t end;
	const unsigned short flags;
	const char* description;
} unicode_block_t;

extern const unicode_block_t blocks[];
extern const size_t codepoints;

extern unsigned char* decode64(char **src, size_t *len);
extern unsigned char* decodeqp(char **src);
