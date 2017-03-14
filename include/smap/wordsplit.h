/* wordsplit - a word splitter
   Copyright (C) 2009-2010, 2014 Sergey Poznyakoff

   This program is free software; you can redistribute it and/or modify it
   under the terms of the GNU General Public License as published by the
   Free Software Foundation; either version 3 of the License, or (at your
   option) any later version.

   This program is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
   GNU General Public License for more details.

   You should have received a copy of the GNU General Public License along
   with this program. If not, see <http://www.gnu.org/licenses/>. */

#ifndef __SMAP_WORDSPLIT_H
#define __SMAP_WORDSPLIT_H

#include <stddef.h>

struct wordsplit {
	size_t ws_wordc;
	char **ws_wordv;
	size_t ws_offs;
	size_t ws_wordn;
	int ws_flags;
	const char *ws_delim;
	const char *ws_comment;
	void (*ws_alloc_die)(struct wordsplit *wsp);
	void (*ws_error)(const char *, ...)
		__attribute__ ((__format__ (__printf__, 1, 2)));
	void (*ws_debug)(const char *, ...)
		__attribute__ ((__format__ (__printf__, 1, 2)));
	
	const char **ws_env;
	char *(*ws_getvar) (const char *, size_t, void *); 
	void *ws_closure;
	
	const char *ws_input;
	size_t ws_len;
	size_t ws_endp;
	int ws_errno;
	struct wordsplit_node *ws_head, *ws_tail;
};

/* Append  the words found to the array resulting from a previous
   call. */
#define WRDSF_APPEND            0x0000001
/* Insert we_offs initial NULLs in the array ws_wordv.
   (These are not counted in the returned ws_wordc.) */
#define WRDSF_DOOFFS            0x0000002
/* Don't do command substitution. Reserved for future use. */
#define WRDSF_NOCMD             0x0000004
/* The parameter p resulted from a previous call to
   wordsplit(), and wordsplit_free() was not called. Reuse the
   allocated storage. */
#define WRDSF_REUSE             0x0000008
/* Print errors */
#define WRDSF_SHOWERR           0x0000010
/* Consider it an error if an undefined shell variable
   is expanded. */
#define WRDSF_UNDEF             0x0000020

/* Don't do variable expansion. */
#define WRDSF_NOVAR             0x0000040
/* Abort on ENOMEM error */
#define WRDSF_ENOMEMABRT        0x0000080
/* Trim off any leading and trailind whitespace */
#define WRDSF_WS                0x0000100
/* Handle quotes and escape directives */
#define WRDSF_QUOTE             0x0000200
/* Replace each input sequence of repeated delimiters with a single
   delimiter */
#define WRDSF_SQUEEZE_DELIMS    0x0000400
/* Return delimiters */
#define WRDSF_RETURN_DELIMS     0x0000800
/* Treat sed expressions as words */
#define WRDSF_SED_EXPR          0x0001000
/* ws_delim field is initialized */
#define WRDSF_DELIM             0x0002000
/* ws_comment field is initialized */
#define WRDSF_COMMENT           0x0004000
/* ws_alloc_die field is initialized */
#define WRDSF_ALLOC_DIE         0x0008000
/* ws_error field is initialized */
#define WRDSF_ERROR             0x0010000
/* ws_debug field is initialized */
#define WRDSF_DEBUG             0x0020000
/* ws_env field is initialized */
#define WRDSF_ENV               0x0040000
/* ws_getvar field is initialized */
#define WRDSF_GETVAR            0x0080000
/* enable debugging */
#define WRDSF_SHOWDBG           0x0100000
/* Don't split input into words.  Useful for side effects. */
#define WRDSF_NOSPLIT           0x0200000
/* Keep undefined variables in place, instead of expanding them to
   empty string */
#define WRDSF_KEEPUNDEF         0x0400000
/* Warn about undefined variables */
#define WRDSF_WARNUNDEF         0x0800000
/* Handle C escapes */
#define WRDSF_CESCAPES          0x1000000

/* ws_closure is set */
#define WRDSF_CLOSURE           0x2000000
/* ws_env is a Key/Value environment, i.e. the value of a variable is
   stored in the element that follows its name. */
#define WRDSF_ENV_KV            0x4000000

#define WRDSF_DEFFLAGS	       \
  (WRDSF_NOVAR | WRDSF_NOCMD | \
   WRDSF_QUOTE | WRDSF_SQUEEZE_DELIMS | WRDSF_CESCAPES)

#define WRDSE_EOF        0
#define WRDSE_QUOTE      1
#define WRDSE_NOSPACE    2
#define WRDSE_NOSUPP     3
#define WRDSE_USAGE      4
#define WRDSE_CBRACE     5
#define WRDSE_UNDEF      6

int wordsplit(const char *s, struct wordsplit *p, int flags);
int wordsplit_len(const char *s, size_t len,
		  struct wordsplit *p, int flags);
void wordsplit_free(struct wordsplit *p);
void wordsplit_free_words(struct wordsplit *ws);

int wordsplit_c_unquote_char(int c);
int wordsplit_c_quote_char(int c);
size_t wordsplit_c_quoted_length(const char *str, int quote_hex,
				 int *quote);
void wordsplit_sh_unquote_copy(char *dst, const char *src, size_t n);
void wordsplit_c_unquote_copy(char *dst, const char *src, size_t n);
void wordsplit_c_quote_copy(char *dst, const char *src, int quote_hex);

int wordsplit_varnames(const char *input, char ***ret_names, int af);

void wordsplit_perror(struct wordsplit *ws);
const char *wordsplit_strerror(struct wordsplit *ws);


#endif
