/*
 * mimelang.c
 *
 * Copyright 2021 Werner Fink, 2021 SUSE Software Solutions Germany GmbH.
 *
 * This source is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 */

#ifndef _GNU_SOURCE
# define _GNU_SOURCE
#endif
#include <unistd.h>
#include <getopt.h>
#include <limits.h>
#include <locale.h>
#include <stddef.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/types.h>
#include <wchar.h>

#include "mimelang.h"

#undef MB_CUR_MAX
#define MB_CUR_MAX MB_LEN_MAX

#define F 0    /* character never appears in mail text */
#define T 1    /* character appears in plain ASCII text */
#define I 2    /* character appears in ISO-8859 text */
#define X 3    /* character appears in non-ISO extended ASCII (Mac, IBM PC) */

static char text_chars[256] = {
    /* NUL		  BEL BS HT LF    FF CR    */
	F, F, F, F, F, F, F, F, T, T, T, F, T, T, F, F,  /* 0x0X */
	/*			      ESC	  */
	F, F, F, F, F, F, F, F, F, F, F, T, F, F, F, F,  /* 0x1X */
	T, T, T, T, T, T, T, T, T, T, T, T, T, T, T, T,  /* 0x2X */
	T, T, T, T, T, T, T, T, T, T, T, T, T, T, T, T,  /* 0x3X */
	T, T, T, T, T, T, T, T, T, T, T, T, T, T, T, T,  /* 0x4X */
	T, T, T, T, T, T, T, T, T, T, T, T, T, T, T, T,  /* 0x5X */
	T, T, T, T, T, T, T, T, T, T, T, T, T, T, T, T,  /* 0x6X */
	T, T, T, T, T, T, T, T, T, T, T, T, T, T, T, F,  /* 0x7X */
	/*	    NEL			    */
	X, X, X, X, X, X, X, X, X, X, X, X, X, X, X, X,  /* 0x8X */
	X, X, X, X, X, X, X, X, X, X, X, X, X, X, X, X,  /* 0x9X */
	I, I, I, I, I, I, I, I, I, I, I, I, I, I, I, I,  /* 0xaX */
	I, I, I, I, I, I, I, I, I, I, I, I, I, I, I, I,  /* 0xbX */
	I, I, I, I, I, I, I, I, I, I, I, I, I, I, I, I,  /* 0xcX */
	I, I, I, I, I, I, I, I, I, I, I, I, I, I, I, I,  /* 0xdX */
	I, I, I, I, I, I, I, I, I, I, I, I, I, I, I, I,  /* 0xeX */
	I, I, I, I, I, I, I, I, I, I, I, I, I, I, I, I   /* 0xfX */
};


static int options;
static int encflags;
static int
test_enc(const char *s, size_t len)
{
	int c = *s;
	if (c & 0100) {
		int n, follow, p;
		wchar_t wc = 0;
		size_t wret;

		if      ((c & 040) == 0)	/* 110xxxxx */
			follow = 1;
		else if ((c & 020) == 0)	/* 1110xxxx */
			follow = 2;
		else if ((c & 010) == 0)	/* 11110xxx */
			follow = 3;
		else if ((c & 004) == 0)	/* 111110xx */
			follow = 4;
		else if ((c & 002) == 0)	/* 1111110x */
			follow = 5;
		else
			goto latin;

		for (n = 1; n <= follow; n++) {
			if (n > len)
				goto latin;
			if ((c = *(s+n)) == '\0')
				goto latin;
			if ((c & 0200) == 0 || (c & 0100))
				goto latin;
		}
		encflags = MIME_UTF8;
		(void) mblen ((char *) NULL, 0);
		wret = mbtowc(&wc, s, follow+1);
		switch (wret) {
		case (size_t)-1:
			fprintf(stderr, "Invalid multi byte character: %m\n");
			break;
		case (size_t)-2:
			fprintf(stderr, "Incomplete multi byte character\n");
			break;
		default:
			if (options & 0002)
				printf("U+%04x %lc", wc, wc);
			for (p = 0; p < codepoints; p++) {
				if (blocks[p].start <= wc && wc <= blocks[p].end) {
					encflags |= blocks[p].flags;
					if (options & 0004)
						printf("\t%s", blocks[p].description);
					break;
				}
			}
			if (options & 0002)
				putchar('\n');
			break;
		}
		return follow;
	}
latin:
	c = *s;
	if (text_chars[c & 0377] == I) {
		encflags = MIME_LATIN;
		if (options & 0010)
			printf("L+%04x %c\n", (unsigned char)c, (char)c);
	} else {
		if (options & 0010)
			printf("A+%04x %c\n", (unsigned char)c, c);
	}
	return 1;
}

static size_t
strchrcnt(const char* src, const int c)
{
	size_t num = 0;
	char *ptr = (char*)src;

	while ((ptr = strchr(ptr, c)) != NULL) {
		num++;
		ptr++;
	}

	return num;
}

static int
has_highbit(const char *s, size_t len)
{
	if (s) {
		do {
			if (*s & 0200) {
				int off = test_enc(s, len);
				s += off;
				len -= off;
			} else {
				if (options & 0010)
					printf("A+%04x %c\n", *s, *s);
			}
			s++;
			len--;
		} while (*s != '\0');
	}
	return 0;
}

static void
decode_quoted_printable(const char *body)
{
	size_t len, nsign;
	char *buffer;
	unsigned char *out;
	char *ptr;
	ssize_t pos;

	if (!body)
		return;

	nsign  = strchrcnt(body, '=');
	len = strlen(body);

	len = len - (3*nsign) + nsign + 4;

	buffer = (char*)malloc(len);
	if (!buffer) {
		perror("decode_quoted_printable: ");
		exit(1);
	}
		

	memset(buffer, 0, len);
	ptr = (char*)body;
	pos = 0;

	while (pos < len && (out = decodeqp(&ptr)))
		buffer[pos++] = *(char*)out;
	buffer[pos] = '\0';

	if (buffer[0] != '\0')
		has_highbit(buffer, pos);

	free(buffer);
}

static void
decode_base64(const char *body)
{
	size_t len = strlen(body), off;
	char buffer[len+4];
	unsigned char *out;
	char *ptr;
	ssize_t pos;

	if (!body)
		return;

	memset(buffer, 0, sizeof(buffer));
	ptr = (char*)body;
	pos = 0;

	while (pos < sizeof(buffer) && (out = decode64(&ptr, &off))) {
		strncpy(&buffer[pos], (char*)out, off);
		pos += off;
	}
	buffer[pos] = '\0';

	if (buffer[0] != '\0')
		has_highbit(buffer, pos);
}

int main(int argc, char *argv[])
{
	char *line = NULL, *ptr;
	size_t len = 0;
	int opt;

	setlocale(LC_CTYPE, "en_US.UTF-8");	// Required to use any MB function of libc

	while ((opt = getopt(argc, argv, "V::vh")) != -1) {
		switch (opt) {
		case 'v':
			options |= 0001;
			break;
		case 'V':
			options |= 0002;
			if (optarg && strchr(optarg, 'u'))
				options |= 0004;
			if (optarg && strchr(optarg, 'a'))
				options |= 0010;
			break;
		case 'h':
		default:
			fprintf(stderr, "Usage: e.g. echo '=?utf-8?Q?<Ouoted Printable>?=' | %s [-v] [-V[u]]\n", argv[0]);
			return 0;
		}
	}

	ptr = line;
	while (getline(&line, &len, stdin) != -1) {
		char *start = &line[0];
		/*
		 * Let us seek for encoding start =?utf-8?Q? for quoted printables or
		 * =?utf-8?B? for base64 strings. The encoding stop is marked with ?=
		 */
		while ((start = strcasestr(start, "=?utf-8?")) && *(start+9) == '?') {
			char *end;
			start += 8;
			if ((end = strcasestr(start+2, "?=")))
				*end = '\0';
			if (*start == 'Q' || *start == 'q')
				decode_quoted_printable(start+2);
			if (*start == 'B' || *start == 'b')
				decode_base64(start+2);
			start = end+2;
		}
	}

	if (options & 0001) {
		if (!encflags)
			printf("ASCII string\n");
		else if ((encflags & (MIME_LATIN|MIME_UTF8)) == MIME_LATIN)
			printf("Latin string\n");
		else if ((encflags & (MIME_LATIN|MIME_UTF8)) == (MIME_LATIN|MIME_UTF8))
			printf("UTF-8 string\n");
		else if (encflags & MIME_UTF8) {
			printf("UTF-8 string");
			if (encflags & MIME_NONEU)
				printf(" none europe\n");
			else
				putchar('\n');
		}
	}

	if (ptr)
		free(ptr);

	return (encflags & MIME_NONEU) ? 1 : 0;
}
