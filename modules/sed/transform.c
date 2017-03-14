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

#ifdef HAVE_CONFIG_H
# include <config.h>
#endif
#include <stdlib.h>
#include <regex.h>
#include <errno.h>
#include <string.h>
#include <smap/stream.h>
#include "transform.h"

enum transform_type
  {
    transform_incomplete,
    transform_first,
    transform_global
  };

enum replace_segm_type
  {
    segm_literal,   /* Literal segment */
    segm_backref,   /* Back-reference segment */
    segm_case_ctl   /* Case control segment (GNU extension) */
  };

enum case_ctl_type
  {
    ctl_stop,       /* Stop case conversion */ 
    ctl_upcase_next,/* Turn the next character to uppercase */ 
    ctl_locase_next,/* Turn the next character to lowercase */
    ctl_upcase,     /* Turn the replacement to uppercase until ctl_stop */
    ctl_locase      /* Turn the replacement to lowercase until ctl_stop */
  };

struct replace_segm
{
  struct replace_segm *next;
  enum replace_segm_type type;
  union
  {
    struct
    {
      char *ptr;
      size_t size;
    } literal;                /* type == segm_literal */   
    size_t ref;               /* type == segm_backref */
    enum case_ctl_type ctl;   /* type == segm_case_ctl */ 
  } v;
};

struct transform_expr
{
  struct transform_expr *next;
  enum transform_type transform_type;
  unsigned match_number;
  regex_t regex;
  /* Compiled replacement expression */
  struct replace_segm *repl_head, *repl_tail;
  size_t segm_count; /* Number of elements in the above list */
};

void
transform_init (struct transform *tr)
{
  tr->tr_head = tr->tr_tail = NULL;
  tr->tr_cctl_buf = NULL;
  tr->tr_cctl_size = 0;
  
  tr->tr_errno = 0;
  tr->tr_errpos = 0;
  tr->tr_errarg = NULL;
  tr->tr_mem = NULL;
}


static void *
tr_malloc (struct transform *tr, size_t size)
{
  void *p = calloc (1, size);
  if (!p)
    {
      errno = ENOMEM;
      tr->tr_errno = TRE_NOMEM;
    }
  return p;
}

static struct transform_expr *
new_transform_expr (struct transform *tr)
{
  struct transform_expr *p = tr_malloc (tr, sizeof *p);
  if (p)
    {
      p->transform_type = transform_incomplete;
      if (tr->tr_tail)
	tr->tr_tail->next = p;
      else
	tr->tr_head = p;
      tr->tr_tail = p;
    }
  return p;
}

static struct replace_segm *
add_segment (struct transform *tr, struct transform_expr *tf)
{
  struct replace_segm *segm = tr_malloc (tr, sizeof *segm);
  if (segm)
    {
      segm->next = NULL;
      if (tf->repl_tail)
	tf->repl_tail->next = segm;
      else
	tf->repl_head = segm;
      tf->repl_tail = segm;
      tf->segm_count++;
    }
  return segm;
}

static int
add_literal_segment (struct transform *tr,
		     struct transform_expr *tf, char *str, char *end)
{
  size_t len = end - str;
  if (len)
    {
      struct replace_segm *segm = add_segment (tr, tf);
      if (!segm)
	return 1;
      segm->type = segm_literal;
      segm->v.literal.ptr = tr_malloc (tr, len + 1);
      if (!segm->v.literal.ptr)
	return 1;
      memcpy (segm->v.literal.ptr, str, len);
      segm->v.literal.ptr[len] = 0;
      segm->v.literal.size = len;
    }
  return 0;
}

static int
add_char_segment (struct transform *tr, struct transform_expr *tf, int chr)
{
  struct replace_segm *segm = add_segment (tr, tf);
  if (segm)
    {
      segm->type = segm_literal;
      segm->v.literal.ptr = tr_malloc (tr, 2);
      if (!segm->v.literal.ptr)
	return 1;
      segm->v.literal.ptr[0] = chr;
      segm->v.literal.ptr[1] = 0;
      segm->v.literal.size = 1;
      return 0;
    }
  return 1;
}

static int
add_backref_segment (struct transform *tr,
		     struct transform_expr *tf, size_t ref)
{
  struct replace_segm *segm = add_segment (tr, tf);
  if (segm)
    {
      segm->type = segm_backref;
      segm->v.ref = ref;
      return 0;
    }
  return 1;
}

static int
add_case_ctl_segment (struct transform *tr,
		      struct transform_expr *tf, enum case_ctl_type ctl)
{
  struct replace_segm *segm = add_segment (tr, tf);
  if (segm)
    {
      segm->type = segm_case_ctl;
      segm->v.ctl = ctl;
      return 0;
    }
  return 1;
}

static void
replace_segm_free (struct replace_segm *segm)
{
  while (segm)
    {
      struct replace_segm *next = segm->next;
      switch (segm->type)
	{
	case segm_literal:
	  free (segm->v.literal.ptr);
	  break;

	case segm_backref:
	case segm_case_ctl:
	  break;
	}
      free (segm);
      segm = next;
    }
}

static void
transform_expr_free (struct transform_expr *xform)
{
  while (xform)
    {
      struct transform_expr *next = xform->next;
      if (xform->transform_type != transform_incomplete)
	regfree (&xform->regex);
      replace_segm_free (xform->repl_head);
      free (xform);
      xform = next;
    }
}

static int
parse_transform_expr (struct transform *tr, const char *expr,
		      int cflags, const char **endp)
{
  int delim;
  int i, j, rc;
  char *str, *beg, *cur;
  const char *p;
  enum transform_type transform_type;
  struct transform_expr *tf = new_transform_expr (tr);

  if (!tf)
    return 1;
  
  if (expr[0] != 's')
    {
      tr->tr_errno = TRE_INVEXP;
      tr->tr_errpos = 0;
      tr->tr_errarg = expr;
      return 1;
    }

  delim = expr[1];

  /* Scan regular expression */
  for (i = 2; expr[i] && expr[i] != delim; i++)
    if (expr[i] == '\\' && expr[i+1])
      i++;

  if (expr[i] != delim)
    {
      tr->tr_errno = TRE_DELIM;
      tr->tr_errpos = i;
      tr->tr_errarg = expr;
      return 1;
    }

  /* Scan replacement expression */
  for (j = i + 1; expr[j] && expr[j] != delim; j++)
    if (expr[j] == '\\' && expr[j+1])
      j++;

  if (expr[j] != delim)
    {
      tr->tr_errno = TRE_TRAIL;
      tr->tr_errpos = j;
      tr->tr_errarg = expr;
      return 1;
    }

  /* Check flags */
  transform_type = transform_first;
  for (p = expr + j + 1; *p && *p != ';'; p++)
    switch (*p)
      {
      case 'g':
	transform_type = transform_global;
	break;

      case 'i':
	cflags |= REG_ICASE;
	break;

      case 'x':
	cflags |= REG_EXTENDED;
	break;
	
      case '0': case '1': case '2': case '3': case '4':
      case '5': case '6': case '7': case '8': case '9':
	tf->match_number = strtoul (p, (char**) &p, 0);
	p--;
	break;

      default:
	tr->tr_errno = TRE_BADFLAG;
	tr->tr_errpos = p - expr;
	tr->tr_errarg = expr;
	return 1;
    }

  if (*p == ';')
    p++;
  
  /* Extract and compile regex */
  str = tr_malloc (tr, i - 1);
  if (!str)
    return 1;
  memcpy (str, expr + 2, i - 2);
  str[i - 2] = 0;

  rc = regcomp (&tf->regex, str, cflags);
  tf->transform_type = transform_type;
  if (rc)
    {
      char errbuf[512];
      regerror (rc, &tf->regex, errbuf, sizeof (errbuf));
      tr->tr_errno = TRE_INVEXP;
      tr->tr_errpos = 0;
      tr->tr_mem = strdup(errbuf);
      tr->tr_errarg = tr->tr_mem;
      free (str);
      return 1;
    }

  if (str[0] == '^' || str[strlen (str) - 1] == '$')
    tf->transform_type = transform_first;
  
  free (str);

  /* Extract and compile replacement expr */
  i++;
  str = tr_malloc (tr, j - i + 1);
  if (!str)
    return 1;
  tr->tr_mem = str;
  memcpy (str, expr + i, j - i);
  str[j - i] = 0;

  for (cur = beg = str; *cur;)
    {
      if (*cur == '\\')
	{
	  size_t n;
	  
	  if (add_literal_segment (tr, tf, beg, cur))
	    return 1;
	  switch (*++cur)
	    {
	    case '0': case '1': case '2': case '3': case '4':
	    case '5': case '6': case '7': case '8': case '9':
	      n = strtoul (cur, &cur, 10);
	      if (n > tf->regex.re_nsub)
		{
		  tr->tr_errno = TRE_BACKREF;
		  tr->tr_errpos = cur - str;
		  tr->tr_errarg = tr->tr_mem;
		  return 1;
		}
	      if (add_backref_segment (tr, tf, n))
		return 1;
	      break;

	    case '\\':
	      if (add_char_segment (tr, tf, '\\'))
		return 1;
	      cur++;
	      break;

	    case 'a':
	      if (add_char_segment (tr, tf, '\a'))
		return 1;
	      cur++;
	      break;
	      
	    case 'b':
	      if (add_char_segment (tr, tf, '\b'))
		return 1;
	      cur++;
	      break;
	      
	    case 'f':
	      if (add_char_segment (tr, tf, '\f'))
		return 1;
	      cur++;
	      break;
	      
	    case 'n':
	      if (add_char_segment (tr, tf, '\n'))
		return 1;
	      cur++;
	      break;
	      
	    case 'r':
	      if (add_char_segment (tr, tf, '\r'))
		return 1;
	      cur++;
	      break;
	      
	    case 't':
	      if (add_char_segment (tr, tf, '\t'))
		return 1;
	      cur++;
	      break;
	      
	    case 'v':
	      if (add_char_segment (tr, tf, '\v'))
		return 1;
	      cur++;
	      break;

	    case '&':
	      if (add_char_segment (tr, tf, '&'))
		return 1;
	      cur++;
	      break;
	      
	    case 'L':
	      /* Turn the replacement to lowercase until a `\U' or `\E'
		 is found, */
	      if (add_case_ctl_segment (tr, tf, ctl_locase))
		return 1;
	      cur++;
	      break;
 
	    case 'l':
	      /* Turn the next character to lowercase, */
	      if (add_case_ctl_segment (tr, tf, ctl_locase_next))
		return 1;
	      cur++;
	      break;
	      
	    case 'U':
	      /* Turn the replacement to uppercase until a `\L' or `\E'
		 is found, */
	      if (add_case_ctl_segment (tr, tf, ctl_upcase))
		return 1;
	      cur++;
	      break;
	      
	    case 'u':
	      /* Turn the next character to uppercase, */
	      if (add_case_ctl_segment (tr, tf, ctl_upcase_next))
		return 1;
	      cur++;
	      break;
	      
	    case 'E':
	      /* Stop case conversion started by `\L' or `\U'. */
	      if (add_case_ctl_segment (tr, tf, ctl_stop))
		return 1;
	      cur++;
	      break;
  
	    default:
	      /* Try to be nice */
	      {
		char buf[2];
		buf[0] = '\\';
		buf[1] = *cur;
		if (add_literal_segment (tr, tf, buf, buf + 2))
		  return 1;
	      }
	      cur++;
	      break;
	    }
	  beg = cur;
	}
      else if (*cur == '&')
	{
	  if (add_literal_segment (tr, tf, beg, cur))
	    return 1;
	  if (add_backref_segment (tr, tf, 0))
	    return 1;
	  beg = ++cur;
	}
      else
	cur++;
    }
  if (add_literal_segment (tr, tf, beg, cur))
    return 1;
  *endp = p;
  free (tr->tr_mem);
  tr->tr_mem = NULL;
  return 0;
}

int
transform_compile (struct transform *tr, const char *expr, int cflags)
{
  transform_init (tr);
  while (*expr)
    if (parse_transform_expr (tr, expr, cflags, &expr))
      {
	transform_expr_free (tr->tr_head);
	tr->tr_head = tr->tr_tail = NULL;
	return 1;
      }
  return 0;
}

int
transform_compile_incr (struct transform *tr, const char *expr, int cflags)
{
  while (*expr)
    if (parse_transform_expr (tr, expr, cflags, &expr))
      {
	transform_expr_free (tr->tr_head);
	tr->tr_head = tr->tr_tail = NULL;
	return 1;
      }
  return 0;
}

#define ISUPPER(c) ('A' <= ((unsigned) (c)) && ((unsigned) (c)) <= 'Z')
#define ISLOWER(c) ('a' <= ((unsigned) (c)) && ((unsigned) (c)) <= 'z')
#define TOUPPER(c) (ISLOWER(c) ? ((c) + 'A' - 'a') : (c)) 
#define TOLOWER(c) (ISUPPER(c) ? ((c) + 'a' - 'A') : (c))

/* Run case conversion specified by CASE_CTL on array PTR of SIZE
   characters. Returns pointer to statically allocated storage. */
static char *
run_case_conv (struct transform *tr, enum case_ctl_type case_ctl,
	       const char *ptr, size_t size)
{
  char *p;
  
  if (tr->tr_cctl_size < size)
    {
      char *newbuf = realloc (tr->tr_cctl_buf, size);
      if (!newbuf)
	return NULL;
      tr->tr_cctl_size = size;
      tr->tr_cctl_buf = newbuf;
    }
  memcpy (tr->tr_cctl_buf, ptr, size);
  switch (case_ctl)
    {
    case ctl_upcase_next:
      tr->tr_cctl_buf[0] = TOUPPER (tr->tr_cctl_buf[0]);
      break;
      
    case ctl_locase_next:
      tr->tr_cctl_buf[0] = TOLOWER (tr->tr_cctl_buf[0]);
      break;
      
    case ctl_upcase:
      for (p = tr->tr_cctl_buf; p < tr->tr_cctl_buf + size; p++)
	*p = TOUPPER (*p);
      break;
      
    case ctl_locase:
      for (p = tr->tr_cctl_buf; p < tr->tr_cctl_buf + size; p++)
	*p = TOLOWER (*p);
      break;

    case ctl_stop:
      break;
    }
  return tr->tr_cctl_buf;
}


static int
transform_append (struct transform *tr, void *slist, const char *str,
		  int len)
{
  if (tr->tr_append (slist, str, len))
    {
      tr->tr_errno = TRE_NOMEM;
      return 1;
    }
  return 0;
}

static int
_single_transform_name_to_slist (struct transform *tr,
				 struct transform_expr *tf,
				 const char *input,
				 void *slist)
{
  regmatch_t *rmp;
  int rc;
  size_t nmatches = 0;
  enum case_ctl_type case_ctl = ctl_stop,  /* Current case conversion op */
                     save_ctl = ctl_stop;  /* Saved case_ctl for \u and \l */
  
  /* Reset case conversion after a single-char operation */
#define CASE_CTL_RESET()  if (case_ctl == ctl_upcase_next     \
			      || case_ctl == ctl_locase_next) \
                            {                                 \
                              case_ctl = save_ctl;            \
                              save_ctl = ctl_stop;            \
			    }
  
  rmp = tr_malloc (tr, (tf->regex.re_nsub + 1) * sizeof (*rmp));
  if (!rmp)
    return 1;

  while (*input)
    {
      size_t disp;
      const char *ptr;
      
      rc = regexec (&tf->regex, input, tf->regex.re_nsub + 1, rmp, 0);
      
      if (rc == 0)
	{
	  struct replace_segm *segm;
	  
	  disp = rmp[0].rm_eo;

	  if (rmp[0].rm_so)
	    if (transform_append (tr, slist, input, rmp[0].rm_so))
	      return 1;

	  nmatches++;
	  if (tf->match_number && nmatches < tf->match_number)
	    {
	      if (transform_append (tr, slist, input, disp))
		return 1;
	      input += disp;
	      continue;
	    }

	  for (segm = tf->repl_head; segm; segm = segm->next)
	    {
	      switch (segm->type)
		{
		case segm_literal:    /* Literal segment */
		  if (case_ctl == ctl_stop)
		    ptr = segm->v.literal.ptr;
		  else
		    {
		      ptr = run_case_conv (tr,
					   case_ctl,
					   segm->v.literal.ptr,
					   segm->v.literal.size);
		      if (!ptr)
			return 1;
		      CASE_CTL_RESET();
		    }
		  if (transform_append (tr, slist, ptr, segm->v.literal.size))
		    return 1;
		  break;
	      
		case segm_backref:    /* Back-reference segment */
		  if (rmp[segm->v.ref].rm_so != -1
		      && rmp[segm->v.ref].rm_eo != -1)
		    {
		      size_t size = rmp[segm->v.ref].rm_eo
			              - rmp[segm->v.ref].rm_so;
		      ptr = input + rmp[segm->v.ref].rm_so;
		      if (case_ctl != ctl_stop)
			{
			  ptr = run_case_conv (tr, case_ctl, ptr, size);
			  if (!ptr)
			    return 1;
			  CASE_CTL_RESET();
			}
		      
		      if (transform_append (tr, slist, ptr, size))
			return 1;
		    }
		  break;

		case segm_case_ctl:
		  switch (segm->v.ctl)
		    {
		    case ctl_upcase_next:
		    case ctl_locase_next:
		      switch (save_ctl)
			{
			case ctl_stop:
			case ctl_upcase:
			case ctl_locase:
			  save_ctl = case_ctl;
			default:
			  break;
			}
		      /*FALL THROUGH*/
		      
		    case ctl_upcase:
		    case ctl_locase:
		    case ctl_stop:
		      case_ctl = segm->v.ctl;
		    }
		}
	    }
	}
      else
	{
	  disp = strlen (input);
	  if (transform_append (tr, slist, input, disp))
	    return 1;
	}

      input += disp;

      if (tf->transform_type == transform_first)
	{
	  if (transform_append (tr, slist, input, strlen (input)))
	    return 1;
	  break;
	}
    }

  free (rmp);
  return 0;
}

int
transform_string (struct transform *tr, const char *input,
		  void *slist,
		  char **poutput)
{
  struct transform_expr *tf;
  char *output = NULL;
  
  tr->tr_errno = 0;
  tr->tr_errpos = 0;
  tr->tr_errarg = NULL;
  tr->tr_mem = NULL;

  if (!tr->tr_append || !tr->tr_reduce)
    {
      tr->tr_errno = TRE_INVAL;
      return 1;
    }
  
  for (tf = tr->tr_head; tf; tf = tf->next)
    {
      if (_single_transform_name_to_slist (tr, tf, input, slist))
	return 1;
      if (tr->tr_append (slist, "", 1)
	  || tr->tr_reduce (slist, &output))
	{
	  tr->tr_errno = TRE_NOMEM;
	  return 1;
	}
    }
  *poutput = output;
  return 0;
}

void
transform_free (struct transform *tr)
{
  transform_expr_free (tr->tr_head);
  tr->tr_head = tr->tr_tail = NULL;
  if (tr->tr_cctl_buf)
    {
      free (tr->tr_cctl_buf);
      tr->tr_cctl_buf = NULL;
      tr->tr_cctl_size = 0;
    }
  if (tr->tr_mem)
    {
      free (tr->tr_mem);
      tr->tr_mem = NULL;
    }
}


const char *_transform_errstr[] = {
  "invalid transform structure",
  "not enough memory",
  "invalid input expression",
  "missing 2nd delimiter",
  "missing trailing delimiter",
  "bad flag",
  "invalid backreference",
};

int _transform_nerrs = sizeof(_transform_errstr)/sizeof(_transform_errstr[0]);


const char *
transform_strerror(struct transform *tr)
{
  if (tr->tr_errno < _transform_nerrs)
    return _transform_errstr[tr->tr_errno];
  return "unknown error";
}

void
transform_perror(struct transform *tr, smap_stream_t str)
{
  const char *text = transform_strerror(tr); 
  smap_stream_write(str, text, strlen(text), NULL);
  if (tr->tr_errarg)
    {
      smap_stream_printf(str, ", in \"%s\" pos. %d",
			 tr->tr_errarg,
			 tr->tr_errpos);
    }
}


/*
 Local Variables:
 c-file-style: "gnu"
 End:
*/
/* EOF */

