/* -*- buffer-read-only: t -*- vi: set ro:
   THIS FILE IS GENERATED AUTOMATICALLY.  PLEASE DO NOT EDIT.
*/
#line 1 "cmdline.opt"
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

#line 17 "cmdline.opt"

#line 17
void print_help(void);
#line 17
void print_usage(void);
#line 109 "cmdline.opt"

#line 109
/* Option codes */
#line 109
enum {
#line 109
	_OPTION_INIT=255,
#line 109
	#line 109 "cmdline.opt"

#line 109
	OPTION_USAGE,

#line 109 "cmdline.opt"
	MAX_OPTION
#line 109
};
#line 109
#ifdef HAVE_GETOPT_LONG
#line 109
static struct option long_options[] = {
#line 109
	#line 24 "cmdline.opt"

#line 24
	{ "lint", no_argument, 0, 't' },
#line 31 "cmdline.opt"

#line 31
	{ "inetd", no_argument, 0, 'i' },
#line 38 "cmdline.opt"

#line 38
	{ "config", required_argument, 0, 'c' },
#line 44 "cmdline.opt"

#line 44
	{ "foreground", no_argument, 0, 'f' },
#line 50 "cmdline.opt"

#line 50
	{ "single-process", no_argument, 0, 's' },
#line 58 "cmdline.opt"

#line 58
	{ "stderr", no_argument, 0, 'e' },
#line 64 "cmdline.opt"

#line 64
	{ "syslog", no_argument, 0, 'l' },
#line 70 "cmdline.opt"

#line 70
	{ "log-tag", required_argument, 0, 'L' },
#line 76 "cmdline.opt"

#line 76
	{ "log-facility", required_argument, 0, 'F' },
#line 88 "cmdline.opt"

#line 88
	{ "trace", no_argument, 0, 'T' },
#line 94 "cmdline.opt"

#line 94
	{ "trace-pattern", required_argument, 0, 'p' },
#line 100 "cmdline.opt"

#line 100
	{ "debug", required_argument, 0, 'd' },
#line 109 "cmdline.opt"

#line 109
	{ "help", no_argument, 0, 'h' },
#line 109 "cmdline.opt"

#line 109
	{ "usage", no_argument, 0, OPTION_USAGE },
#line 109 "cmdline.opt"

#line 109
	{ "version", no_argument, 0, 'V' },

#line 109 "cmdline.opt"
	{0, 0, 0, 0}
#line 109
};
#line 109
#endif
#line 109
static struct opthelp {
#line 109
	const char *opt;
#line 109
	const char *arg;
#line 109
	int is_optional;
#line 109
	const char *descr;
#line 109
} opthelp[] = {
#line 109
	#line 23 "cmdline.opt"

#line 23
	{ NULL, NULL, 0, N_("Operation mode") },
#line 26 "cmdline.opt"

#line 26
	{
#line 26
#ifdef HAVE_GETOPT_LONG
#line 26
	  "-t, --lint",
#line 26
#else
#line 26
	  "-t",
#line 26
#endif
#line 26
				   NULL, 0, N_("test configuration and exit") },
#line 33 "cmdline.opt"

#line 33
	{
#line 33
#ifdef HAVE_GETOPT_LONG
#line 33
	  "-i, --inetd",
#line 33
#else
#line 33
	  "-i",
#line 33
#endif
#line 33
				   NULL, 0, N_("inetd mode") },
#line 37 "cmdline.opt"

#line 37
	{ NULL, NULL, 0, N_("Operation modifiers") },
#line 40 "cmdline.opt"

#line 40
	{
#line 40
#ifdef HAVE_GETOPT_LONG
#line 40
	  "-c, --config",
#line 40
#else
#line 40
	  "-c",
#line 40
#endif
#line 40
				   N_("FILE"), 0, N_("read this configuration file") },
#line 46 "cmdline.opt"

#line 46
	{
#line 46
#ifdef HAVE_GETOPT_LONG
#line 46
	  "-f, --foreground",
#line 46
#else
#line 46
	  "-f",
#line 46
#endif
#line 46
				   NULL, 0, N_("operate in foreground") },
#line 52 "cmdline.opt"

#line 52
	{
#line 52
#ifdef HAVE_GETOPT_LONG
#line 52
	  "-s, --single-process",
#line 52
#else
#line 52
	  "-s",
#line 52
#endif
#line 52
				   NULL, 0, N_("single-process mode") },
#line 56 "cmdline.opt"

#line 56
	{ NULL, NULL, 0, N_("Logging") },
#line 60 "cmdline.opt"

#line 60
	{
#line 60
#ifdef HAVE_GETOPT_LONG
#line 60
	  "-e, --stderr",
#line 60
#else
#line 60
	  "-e",
#line 60
#endif
#line 60
				   NULL, 0, N_("output diagnostic to stderr") },
#line 66 "cmdline.opt"

#line 66
	{
#line 66
#ifdef HAVE_GETOPT_LONG
#line 66
	  "-l, --syslog",
#line 66
#else
#line 66
	  "-l",
#line 66
#endif
#line 66
				   NULL, 0, N_("output diagnostic to syslog (default)") },
#line 72 "cmdline.opt"

#line 72
	{
#line 72
#ifdef HAVE_GETOPT_LONG
#line 72
	  "-L, --log-tag",
#line 72
#else
#line 72
	  "-L",
#line 72
#endif
#line 72
				   N_("TAG"), 0, N_("set syslog tag") },
#line 78 "cmdline.opt"

#line 78
	{
#line 78
#ifdef HAVE_GETOPT_LONG
#line 78
	  "-F, --log-facility",
#line 78
#else
#line 78
	  "-F",
#line 78
#endif
#line 78
				   N_("FACILITY"), 0, N_("set syslog facility") },
#line 87 "cmdline.opt"

#line 87
	{ NULL, NULL, 0, N_("Tracing and debugging") },
#line 90 "cmdline.opt"

#line 90
	{
#line 90
#ifdef HAVE_GETOPT_LONG
#line 90
	  "-T, --trace",
#line 90
#else
#line 90
	  "-T",
#line 90
#endif
#line 90
				   NULL, 0, N_("trace queries") },
#line 96 "cmdline.opt"

#line 96
	{
#line 96
#ifdef HAVE_GETOPT_LONG
#line 96
	  "-p, --trace-pattern",
#line 96
#else
#line 96
	  "-p",
#line 96
#endif
#line 96
				   N_("PATTERN"), 0, N_("trace only queries matching this pattern") },
#line 103 "cmdline.opt"

#line 103
	{
#line 103
#ifdef HAVE_GETOPT_LONG
#line 103
	  "-d, -x, --debug",
#line 103
#else
#line 103
	  "-d, -x",
#line 103
#endif
#line 103
				   N_("LEVEL-SPEC"), 0, N_("set debug verbosity level") },
#line 109 "cmdline.opt"

#line 109
	{ NULL, NULL, 0, N_("Other options") },
#line 109 "cmdline.opt"

#line 109
	{
#line 109
#ifdef HAVE_GETOPT_LONG
#line 109
	  "-h, --help",
#line 109
#else
#line 109
	  "-h",
#line 109
#endif
#line 109
				   NULL, 0, N_("Give this help list") },
#line 109 "cmdline.opt"

#line 109
	{
#line 109
#ifdef HAVE_GETOPT_LONG
#line 109
	  "--usage",
#line 109
#else
#line 109
	  "",
#line 109
#endif
#line 109
				   NULL, 0, N_("Give a short usage message") },
#line 109 "cmdline.opt"

#line 109
	{
#line 109
#ifdef HAVE_GETOPT_LONG
#line 109
	  "-V, --version",
#line 109
#else
#line 109
	  "-V",
#line 109
#endif
#line 109
				   NULL, 0, N_("Print program version") },

#line 109 "cmdline.opt"
};
#line 17 "cmdline.opt"

#line 17
const char *program_version = "smapd" " (" PACKAGE_NAME ") " PACKAGE_VERSION;
#line 17
static char doc[] = N_("A socket map daemon.");
#line 17
static char args_doc[] = N_("");
#line 17
const char *program_bug_address = "<" PACKAGE_BUGREPORT ">";
#line 17

#line 17
#define DESCRCOLUMN 30
#line 17
#define RMARGIN 79
#line 17
#define GROUPCOLUMN 2
#line 17
#define USAGECOLUMN 13
#line 17

#line 17
static void
#line 17
indent (size_t start, size_t col)
#line 17
{
#line 17
  for (; start < col; start++)
#line 17
    putchar (' ');
#line 17
}
#line 17

#line 17
static void
#line 17
print_option_descr (const char *descr, size_t lmargin, size_t rmargin)
#line 17
{
#line 17
  while (*descr)
#line 17
    {
#line 17
      size_t s = 0;
#line 17
      size_t i;
#line 17
      size_t width = rmargin - lmargin;
#line 17

#line 17
      for (i = 0; ; i++)
#line 17
	{
#line 17
	  if (descr[i] == 0 || isspace (descr[i]))
#line 17
	    {
#line 17
	      if (i > width)
#line 17
		break;
#line 17
	      s = i;
#line 17
	      if (descr[i] == 0)
#line 17
		break;
#line 17
	    }
#line 17
	}
#line 17
      printf ("%*.*s\n", s, s, descr);
#line 17
      descr += s;
#line 17
      if (*descr)
#line 17
	{
#line 17
	  indent (0, lmargin);
#line 17
	  descr++;
#line 17
	}
#line 17
    }
#line 17
}
#line 17

#line 17
#define NOPTHELP (sizeof (opthelp) / sizeof (opthelp[0]))
#line 17

#line 17
static int
#line 17
optcmp (const void *a, const void *b)
#line 17
{
#line 17
  struct opthelp const *ap = (struct opthelp const *)a;
#line 17
  struct opthelp const *bp = (struct opthelp const *)b;
#line 17
  const char *opta, *optb;
#line 17
  size_t alen, blen;
#line 17

#line 17
  for (opta = ap->opt; *opta == '-'; opta++)
#line 17
    ;
#line 17
  alen = strcspn (opta, ",");
#line 17
  
#line 17
  for (optb = bp->opt; *optb == '-'; optb++)
#line 17
    ;
#line 17
  blen = strcspn (optb, ",");
#line 17

#line 17
  if (alen > blen)
#line 17
    blen = alen;
#line 17
  
#line 17
  return strncmp (opta, optb, blen);
#line 17
}
#line 17

#line 17
static void
#line 17
sort_options (int start, int count)
#line 17
{
#line 17
  qsort (opthelp + start, count, sizeof (opthelp[0]), optcmp);
#line 17
}
#line 17

#line 17
static int
#line 17
sort_group (int start)
#line 17
{
#line 17
  int i;
#line 17
  
#line 17
  for (i = start; i < NOPTHELP && opthelp[i].opt; i++)
#line 17
    ;
#line 17
  sort_options (start, i - start);
#line 17
  return i + 1;
#line 17
}
#line 17

#line 17
static void
#line 17
sort_opthelp (void)
#line 17
{
#line 17
  int start;
#line 17

#line 17
  for (start = 0; start < NOPTHELP; )
#line 17
    {
#line 17
      if (!opthelp[start].opt)
#line 17
	start = sort_group (start + 1);
#line 17
      else 
#line 17
	start = sort_group (start);
#line 17
    }
#line 17
}
#line 17

#line 17
void
#line 17
print_help (void)
#line 17
{
#line 17
  unsigned i;
#line 17
  int argsused = 0;
#line 17

#line 17
  printf ("%s %s [%s]... %s\n", _("Usage:"), "smapd", _("OPTION"),
#line 17
	  gettext (args_doc));
#line 17
  if (doc[0])
#line 17
    print_option_descr(gettext (doc), 0, RMARGIN);
#line 17
  putchar ('\n');
#line 17

#line 17
  sort_opthelp ();
#line 17
  for (i = 0; i < NOPTHELP; i++)
#line 17
    {
#line 17
      unsigned n;
#line 17
      if (opthelp[i].opt)
#line 17
	{
#line 17
	  n = printf ("  %s", opthelp[i].opt);
#line 17
	  if (opthelp[i].arg)
#line 17
	    {
#line 17
	      char *cb, *ce;
#line 17
	      argsused = 1;
#line 17
	      if (strlen (opthelp[i].opt) == 2)
#line 17
		{
#line 17
		  if (!opthelp[i].is_optional)
#line 17
		    {
#line 17
		      putchar (' ');
#line 17
		      n++;
#line 17
		    }
#line 17
		}
#line 17
	      else
#line 17
		{
#line 17
		  putchar ('=');
#line 17
		  n++;
#line 17
		}
#line 17
	      if (opthelp[i].is_optional)
#line 17
		{
#line 17
		  cb = "[";
#line 17
		  ce = "]";
#line 17
		}
#line 17
	      else
#line 17
		cb = ce = "";
#line 17
	      n += printf ("%s%s%s", cb, gettext (opthelp[i].arg), ce);
#line 17
	    }
#line 17
	  if (n >= DESCRCOLUMN)
#line 17
	    {
#line 17
	      putchar ('\n');
#line 17
	      n = 0;
#line 17
	    }
#line 17
	  indent (n, DESCRCOLUMN);
#line 17
	  print_option_descr (gettext (opthelp[i].descr), DESCRCOLUMN, RMARGIN);
#line 17
	}
#line 17
      else
#line 17
	{
#line 17
	  if (i)
#line 17
	    putchar ('\n');
#line 17
	  indent (0, GROUPCOLUMN);
#line 17
	  print_option_descr (gettext (opthelp[i].descr),
#line 17
			      GROUPCOLUMN, RMARGIN);
#line 17
	  putchar ('\n');
#line 17
	}
#line 17
    }
#line 17

#line 17
  putchar ('\n');
#line 17
  if (argsused)
#line 17
    {
#line 17
      print_option_descr (_("Mandatory or optional arguments to long options are also mandatory or optional for any corresponding short options."), 0, RMARGIN);
#line 17
      putchar ('\n');
#line 17
    }
#line 17
 
#line 17
 /* TRANSLATORS: The placeholder indicates the bug-reporting address
    for this package.  Please add _another line_ saying
    "Report translation bugs to <...>\n" with the address for translation
    bugs (typically your translation team's web or email address).  */
#line 17
  printf (_("Report bugs to %s.\n"), program_bug_address);
#line 17
  
#line 17
#ifdef PACKAGE_URL
#line 17
  printf (_("%s home page: <%s>\n"), PACKAGE_NAME, PACKAGE_URL);
#line 17
#endif
#line 17
}
#line 17

#line 17
static int
#line 17
cmpidx_short (const void *a, const void *b)
#line 17
{
#line 17
  unsigned const *ai = (unsigned const *)a;
#line 17
  unsigned const *bi = (unsigned const *)b;
#line 17

#line 17
  return opthelp[*ai].opt[1] - opthelp[*bi].opt[1];
#line 17
}
#line 17
  
#line 17
#ifdef HAVE_GETOPT_LONG
#line 17
static int
#line 17
cmpidx_long (const void *a, const void *b)
#line 17
{
#line 17
  unsigned const *ai = (unsigned const *)a;
#line 17
  unsigned const *bi = (unsigned const *)b;
#line 17
  struct opthelp const *ap = opthelp + *ai;
#line 17
  struct opthelp const *bp = opthelp + *bi;
#line 17
  char const *opta, *optb;
#line 17
  size_t lena, lenb;
#line 17

#line 17
  if (ap->opt[1] == '-')
#line 17
    opta = ap->opt;
#line 17
  else
#line 17
    opta = ap->opt + 4;
#line 17
  lena = strcspn (opta, ",");
#line 17
  
#line 17
  if (bp->opt[1] == '-')
#line 17
    optb = bp->opt;
#line 17
  else
#line 17
    optb = bp->opt + 4;
#line 17
  lenb = strcspn (optb, ",");
#line 17
  return strncmp (opta, optb, lena > lenb ? lenb : lena);
#line 17
}
#line 17
#endif
#line 17

#line 17
void
#line 17
print_usage (void)
#line 17
{
#line 17
  unsigned i;
#line 17
  unsigned n;
#line 17
  char buf[RMARGIN+1];
#line 17
  unsigned *idxbuf;
#line 17
  unsigned nidx;
#line 17
  
#line 17
#define FLUSH                        do                                   {                              	  buf[n] = 0;              	  printf ("%s\n", buf);    	  n = USAGECOLUMN;         	  memset (buf, ' ', n);        }                                while (0)
#line 17
#define ADDC(c)   do { if (n == RMARGIN) FLUSH; buf[n++] = c; } while (0)
#line 17

#line 17
  idxbuf = calloc (NOPTHELP, sizeof (idxbuf[0]));
#line 17
  if (!idxbuf)
#line 17
    {
#line 17
      fprintf (stderr, "not enough memory");
#line 17
      abort ();
#line 17
    }
#line 17

#line 17
  n = snprintf (buf, sizeof buf, "%s %s ", _("Usage:"), "smapd");
#line 17

#line 17
  /* Print a list of short options without arguments. */
#line 17
  for (i = nidx = 0; i < NOPTHELP; i++)
#line 17
      if (opthelp[i].opt && opthelp[i].descr && opthelp[i].opt[1] != '-'
#line 17
	  && opthelp[i].arg == NULL)
#line 17
	idxbuf[nidx++] = i;
#line 17

#line 17
  if (nidx)
#line 17
    {
#line 17
      qsort (idxbuf, nidx, sizeof (idxbuf[0]), cmpidx_short);
#line 17

#line 17
      ADDC ('[');
#line 17
      ADDC ('-');
#line 17
      for (i = 0; i < nidx; i++)
#line 17
	{
#line 17
	  ADDC (opthelp[idxbuf[i]].opt[1]);
#line 17
	}
#line 17
      ADDC (']');
#line 17
    }
#line 17

#line 17
  /* Print a list of short options with arguments. */
#line 17
  for (i = nidx = 0; i < NOPTHELP; i++)
#line 17
    {
#line 17
      if (opthelp[i].opt && opthelp[i].descr && opthelp[i].opt[1] != '-'
#line 17
	  && opthelp[i].arg)
#line 17
	idxbuf[nidx++] = i;
#line 17
    }
#line 17

#line 17
  if (nidx)
#line 17
    {
#line 17
      qsort (idxbuf, nidx, sizeof (idxbuf[0]), cmpidx_short);
#line 17
    
#line 17
      for (i = 0; i < nidx; i++)
#line 17
	{
#line 17
	  struct opthelp *opt = opthelp + idxbuf[i];
#line 17
	  size_t len = 5 + strlen (opt->arg)
#line 17
	                 + (opt->is_optional ? 2 : 1);
#line 17
      
#line 17
	  if (n + len > RMARGIN) FLUSH;
#line 17
	  buf[n++] = ' ';
#line 17
	  buf[n++] = '[';
#line 17
	  buf[n++] = '-';
#line 17
	  buf[n++] = opt->opt[1];
#line 17
	  if (opt->is_optional)
#line 17
	    {
#line 17
	      buf[n++] = '[';
#line 17
	      strcpy (&buf[n], opt->arg);
#line 17
	      n += strlen (opt->arg);
#line 17
	      buf[n++] = ']';
#line 17
	    }
#line 17
	  else
#line 17
	    {
#line 17
	      buf[n++] = ' ';
#line 17
	      strcpy (&buf[n], opt->arg);
#line 17
	      n += strlen (opt->arg);
#line 17
	    }
#line 17
	  buf[n++] = ']';
#line 17
	}
#line 17
    }
#line 17
  
#line 17
#ifdef HAVE_GETOPT_LONG
#line 17
  /* Print a list of long options */
#line 17
  for (i = nidx = 0; i < NOPTHELP; i++)
#line 17
    {
#line 17
      if (opthelp[i].opt && opthelp[i].descr
#line 17
	  &&  (opthelp[i].opt[1] == '-' || opthelp[i].opt[2] == ','))
#line 17
	idxbuf[nidx++] = i;
#line 17
    }
#line 17

#line 17
  if (nidx)
#line 17
    {
#line 17
      qsort (idxbuf, nidx, sizeof (idxbuf[0]), cmpidx_long);
#line 17
	
#line 17
      for (i = 0; i < nidx; i++)
#line 17
	{
#line 17
	  struct opthelp *opt = opthelp + idxbuf[i];
#line 17
	  size_t len;
#line 17
	  const char *longopt;
#line 17
	  
#line 17
	  if (opt->opt[1] == '-')
#line 17
	    longopt = opt->opt;
#line 17
	  else if (opt->opt[2] == ',')
#line 17
	    longopt = opt->opt + 4;
#line 17
	  else
#line 17
	    continue;
#line 17

#line 17
	  len = 3 + strlen (longopt)
#line 17
	          + (opt->arg ? 1 + strlen (opt->arg)
#line 17
		  + (opt->is_optional ? 2 : 0) : 0);
#line 17
	  if (n + len > RMARGIN) FLUSH;
#line 17
	  buf[n++] = ' ';
#line 17
	  buf[n++] = '[';
#line 17
	  strcpy (&buf[n], longopt);
#line 17
	  n += strlen (longopt);
#line 17
	  if (opt->arg)
#line 17
	    {
#line 17
	      buf[n++] = '=';
#line 17
	      if (opt->is_optional)
#line 17
		{
#line 17
		  buf[n++] = '[';
#line 17
		  strcpy (&buf[n], opt->arg);
#line 17
		  n += strlen (opt->arg);
#line 17
		  buf[n++] = ']';
#line 17
		}
#line 17
	      else
#line 17
		{
#line 17
		  strcpy (&buf[n], opt->arg);
#line 17
		  n += strlen (opt->arg);
#line 17
		}
#line 17
	    }
#line 17
	  buf[n++] = ']';
#line 17
	}
#line 17
    }
#line 17
#endif
#line 17
  FLUSH;
#line 17
  free (idxbuf);
#line 17
}
#line 17

#line 17
const char version_etc_copyright[] =
#line 17
  /* Do *not* mark this string for translation.  First %s is a copyright
     symbol suitable for this locale, and second %s are the copyright
     years.  */
#line 17
  "Copyright %s %s %s";
#line 17

#line 17
void
#line 17
print_version_only(const char *program_version, FILE *stream)
#line 17
{
#line 17
  fprintf (stream, "%s\n", program_version);
#line 17
  /* TRANSLATORS: Translate "(C)" to the copyright symbol
     (C-in-a-circle), if this symbol is available in the user's
     locale.  Otherwise, do not translate "(C)"; leave it as-is.  */
#line 17
  fprintf (stream, version_etc_copyright, _("(C)"),
#line 17
	   "2009, 2010, 2014",
#line 17
	   "Sergey Poznyakoff");
#line 17
  fputc ('\n', stream);
#line 17
}
#line 17

#line 17

#line 17

#line 17
void
#line 17
print_version(const char *program_version, FILE *stream)
#line 17
{
#line 17
  
#line 17
  print_version_only(program_version, stream);
#line 17

#line 17
  fputs (_("License GPLv3+: GNU GPL version 3 or later <http://gnu.org/licenses/gpl.html>\nThis is free software: you are free to change and redistribute it.\nThere is NO WARRANTY, to the extent permitted by law.\n\n"),
#line 17
	 stream);
#line 17
		
#line 17
}
#line 17

#line 109 "cmdline.opt"

#line 109


void
get_options (int argc, char *argv[])
{
    
#line 114
 {
#line 114
  int c;
#line 114

#line 114
#ifdef HAVE_GETOPT_LONG
#line 114
  while ((c = getopt_long(argc, argv, "tic:fselL:F:Tp:d:x:hV",
#line 114
			  long_options, NULL)) != EOF)
#line 114
#else
#line 114
  while ((c = getopt(argc, argv, "tic:fselL:F:Tp:d:x:hV")) != EOF)
#line 114
#endif
#line 114
    {
#line 114
      switch (c)
#line 114
	{
#line 114
	default:
#line 114
	   exit(EX_USAGE);	   exit(EX_USAGE);
#line 114
	#line 26 "cmdline.opt"
	 case 't':
#line 26
	  {
#line 26

       lint_mode = 1;
       log_to_stderr = 1;       

#line 29
	     break;
#line 29
	  }
#line 33 "cmdline.opt"
	 case 'i':
#line 33
	  {
#line 33

       optcache_int(&inetd_mode, 1);

#line 35
	     break;
#line 35
	  }
#line 40 "cmdline.opt"
	 case 'c':
#line 40
	  {
#line 40

	config_file = optarg;

#line 42
	     break;
#line 42
	  }
#line 46 "cmdline.opt"
	 case 'f':
#line 46
	  {
#line 46

        optcache_int(&foreground, 1);

#line 48
	     break;
#line 48
	  }
#line 52 "cmdline.opt"
	 case 's':
#line 52
	  {
#line 52

       optcache_int(&srvman_param.single_process, 1);

#line 54
	     break;
#line 54
	  }
#line 60 "cmdline.opt"
	 case 'e':
#line 60
	  {
#line 60

       optcache_int(&log_to_stderr, 1);

#line 62
	     break;
#line 62
	  }
#line 66 "cmdline.opt"
	 case 'l':
#line 66
	  {
#line 66

       optcache_int(&log_to_stderr, 0);

#line 68
	     break;
#line 68
	  }
#line 72 "cmdline.opt"
	 case 'L':
#line 72
	  {
#line 72
       
       optcache_str(&log_tag, optarg);

#line 74
	     break;
#line 74
	  }
#line 78 "cmdline.opt"
	 case 'F':
#line 78
	  {
#line 78

       int n;
       if (syslog_fname_to_n(optarg, &n)) {
            smap_error("unrecognized facility: %s", optarg);
	    exit(EX_USAGE);
       }
       optcache_int(&log_facility, n);

#line 85
	     break;
#line 85
	  }
#line 90 "cmdline.opt"
	 case 'T':
#line 90
	  {
#line 90

       optcache_int(&smap_trace_option, 1);

#line 92
	     break;
#line 92
	  }
#line 96 "cmdline.opt"
	 case 'p':
#line 96
	  {
#line 96

       add_trace_pattern(optarg);

#line 98
	     break;
#line 98
	  }
#line 103 "cmdline.opt"
	 case 'd': case 'x':
#line 103
	  {
#line 103

       if (smap_debug_set(optarg))
           smap_error("invalid debug specification: %s", optarg);

#line 106
	     break;
#line 106
	  }
#line 109 "cmdline.opt"
	 case 'h':
#line 109
	  {
#line 109

#line 109
		print_help ();
#line 109
		exit (0);
#line 109
	 
#line 109
	     break;
#line 109
	  }
#line 109 "cmdline.opt"
	 case OPTION_USAGE:
#line 109
	  {
#line 109

#line 109
		print_usage ();
#line 109
		exit (0);
#line 109
	 
#line 109
	     break;
#line 109
	  }
#line 109 "cmdline.opt"
	 case 'V':
#line 109
	  {
#line 109

#line 109
		/* Give version */
#line 109
		print_version(program_version, stdout);
#line 109
		exit (0);
#line 109
	 
#line 109
	     break;
#line 109
	  }

#line 114 "cmdline.opt"
	}
#line 114
    }
#line 114
  
#line 114
    if (optind < argc) {
#line 114
	fprintf(stderr, "%s: unexpected arguments\n", argv[0]);
#line 114
	exit(EX_USAGE);
#line 114
    }
#line 114

#line 114
 }
#line 114

}
    

