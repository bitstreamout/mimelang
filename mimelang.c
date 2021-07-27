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


static const char base64[] = "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789+/";

__inline__ static const int
isbase64(char c)
{
	return c && strchr(base64, c) != NULL;
}

__inline__ static const char
value(char c)
{
	const char *ptr = strchr(base64, c);
	if (ptr)
		return ptr - base64;
	return 0;
}

static int options;
static int encflags;
static int
test_enc(const char *s, size_t len)
{
	int c = *s;
	if (c & 0100) {
		int n, follow;
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
			if (options & 0002) {
				printf("U+%04x %lc", wc, wc);
				if (options & 0004) {
					int p;
					for (p = 0; p < codepoints; p++) {
						if (blocks[p].start <= wc && wc <= blocks[p].end) {
							encflags |= blocks[p].flags;
							printf("\t%s", blocks[p].description);
							break;
						}
					}
				}
				putchar('\n');
			}
			break;
		}
		return follow;
	}
latin:
	c = *s;
	if (text_chars[c & 0377] == I)
		encflags = MIME_LATIN;
	return 0;
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
			}
			s++;
			len--;
		} while (*s != '\0');
	}
	return 0;
}

static int
hexval(int c)
{
	if ('0' <= c && c <= '9')
		return c - '0';
	if ('A' > c || c > 'F')
		return -1;
	return 10 + c - 'A';
}

static int
decode_char(const char *s)
{
	if (s == NULL || *s != '=')
		return -1;
	if (s+1 == NULL || *(s+1) == '\0')
		return -1;
	if (s+2 == NULL || *(s+2) == '\0')
		return -1;
	return 0x10 * hexval(*(s+1)) + hexval(*(s+2));
}

static void
decode_quoted_printable(const char *body)
{
	char buffer[strlen(body)+4];
	size_t len = sizeof(buffer);
	int pos = 0;

	if (!body)
		return;

	memset(buffer, 0, sizeof(buffer));
	while (*body && pos < len) {
		if (*body != '=')
			buffer[pos++] = *body++;
		else if (*(body+1) == '\r' && *(body+2) == '\n')
			body += 3;
		else if (*(body+1) == '\n')
			body += 2;
		else if (!strchr("0123456789ABCDEF", *(body+1)))
			buffer[pos++] = *body++;
		else if (!strchr("0123456789ABCDEF", *(body+2)))
			buffer[pos++] = *body++;
		else {
		    buffer[pos++] = decode_char(body);
		    body += 3;
		}
	}
	if (buffer[0] != '\0') {
		len = strlen(buffer);
		has_highbit(buffer, len);
	}
}

static void
decode_base64(const char *body)
{
	size_t len = strlen(body);
	char buffer[strlen(body)+4];
	char *ptr;

	if (!body)
		return;

	ptr = &buffer[0];

	memset(buffer, 0, sizeof(buffer));
	do {
		const char a = value(body[0]);
		const char b = value(body[1]);
		const char c = value(body[2]);
		const char d = value(body[3]);

		*ptr++ = (a << 2) | (b >> 4);
		*ptr++ = (b << 4) | (c >> 2);
		*ptr++ = (c << 6) | d;

		if(!isbase64(body[1])) {
			ptr -= 2;
			goto out;
		} 

		if(!isbase64(body[2])) {
			ptr -= 2;
			goto out;
		} 

		if(!isbase64(body[3])) {
			ptr--;
			goto out;
		}

		body += 4;
		while(*body && (*body == '\r' || *body == '\n'))
			body++;

	} while((len-= 4));
out:
	if (buffer[0] != '\0') {
		len = strlen(buffer);
		has_highbit(buffer, len);
	}
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
			if (optarg && *optarg == 'u')
				options |= 0004;
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
			if (*start == 'Q' || *start == 'q') {
				start += 2;
				decode_quoted_printable(start);
			}
			if (*start == 'B' || *start == 'b') {
				start += 2;
				decode_base64(start);
			}
			start = end+2;
		}
	}

	if (options & 0001) {
		if (!encflags)
			printf("ASCII string\n");
		else if (encflags & MIME_LATIN)
			printf("Latin string\n");
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
