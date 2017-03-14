/* This file is part of Smap.
   Copyright (C) 2006-2007, 2010, 2014 Sergey Poznyakoff

   Smap is free software; you can redistribute it and/or modify
   it under the terms of the GNU General Public License as published by
   the Free Software Foundation; either version 3, or (at your option)
   any later version.

   Smap is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
   GNU General Public License for more details.

   You should have received a copy of the GNU General Public License
   along with Smap.  If not, see <http://www.gnu.org/licenses/>. */

#define TRE_INVAL     1      /* invalid transform structure */
#define TRE_NOMEM     2      /* not enough memory */
#define TRE_INVEXP    3      /* invalid input expression */
#define TRE_DELIM     4      /* missing 2nd delimiter */
#define TRE_TRAIL     5      /* missing trailing delimiter */
#define TRE_BADFLAG   6      /* bad flag */
#define TRE_BACKREF   7      /* invalid backreference */

typedef int (*transform_append_t)(void *, const char *, size_t);
typedef int (*transform_reduce_t)(void *, char **);

struct transform
{
  struct transform_expr *tr_head, *tr_tail;

  transform_append_t tr_append;
  transform_reduce_t tr_reduce;

  char *tr_cctl_buf;   /* Case control buffer */
  size_t tr_cctl_size; /* Size of tr_cctl_buf */

  int tr_errno;
  int tr_errpos;
  const char *tr_errarg;
  void *tr_mem;
  char *errstr;
};

void transform_init (struct transform *tr);

int transform_compile (struct transform *tr, const char *expr, int cflags);
int transform_compile_incr (struct transform *tr, const char *expr, int cflags);

int transform_string (struct transform *tr, const char *input,
		      void *slist, char **poutput);

void transform_free (struct transform *tr);

const char *transform_strerror(struct transform *tr);
void transform_perror(struct transform *tr, smap_stream_t str);

