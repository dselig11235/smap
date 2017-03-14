/* -*- buffer-read-only: t -*- vi: set ro:
   THIS FILE IS GENERATED AUTOMATICALLY.  PLEASE DO NOT EDIT.
*/
#line 1 "smapccmd.opt"
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

#line 17 "smapccmd.opt"

#line 17
void print_help(void);
#line 17
void print_usage(void);
#line 77 "smapccmd.opt"

#line 77
/* Option codes */
#line 77
enum {
#line 77
	_OPTION_INIT=255,
#line 77
	#line 77 "smapccmd.opt"

#line 77
	OPTION_USAGE,

#line 77 "smapccmd.opt"
	MAX_OPTION
#line 77
};
#line 77
#ifdef HAVE_GETOPT_LONG
#line 77
static struct option long_options[] = {
#line 77
	#line 24 "smapccmd.opt"

#line 24
	{ "debug", required_argument, 0, 'd' },
#line 32 "smapccmd.opt"

#line 32
	{ "trace", no_argument, 0, 'T' },
#line 38 "smapccmd.opt"

#line 38
	{ "prompt", required_argument, 0, 'p' },
#line 44 "smapccmd.opt"

#line 44
	{ "batch", no_argument, 0, 'B' },
#line 50 "smapccmd.opt"

#line 50
	{ "norc", no_argument, 0, 'q' },
#line 56 "smapccmd.opt"

#line 56
	{ "source", required_argument, 0, 's' },
#line 65 "smapccmd.opt"

#line 65
	{ "server", required_argument, 0, 'S' },
#line 71 "smapccmd.opt"

#line 71
	{ "quiet", no_argument, 0, 'Q' },
#line 77 "smapccmd.opt"

#line 77
	{ "help", no_argument, 0, 'h' },
#line 77 "smapccmd.opt"

#line 77
	{ "usage", no_argument, 0, OPTION_USAGE },
#line 77 "smapccmd.opt"

#line 77
	{ "version", no_argument, 0, 'V' },

#line 77 "smapccmd.opt"
	{0, 0, 0, 0}
#line 77
};
#line 77
#endif
#line 77
static struct opthelp {
#line 77
	const char *opt;
#line 77
	const char *arg;
#line 77
	int is_optional;
#line 77
	const char *descr;
#line 77
} opthelp[] = {
#line 77
	#line 27 "smapccmd.opt"

#line 27
	{
#line 27
#ifdef HAVE_GETOPT_LONG
#line 27
	  "-d, -x, --debug",
#line 27
#else
#line 27
	  "-d, -x",
#line 27
#endif
#line 27
				   N_("LEVEL-SPEC"), 0, N_("set debug verbosity level") },
#line 34 "smapccmd.opt"

#line 34
	{
#line 34
#ifdef HAVE_GETOPT_LONG
#line 34
	  "-T, --trace",
#line 34
#else
#line 34
	  "-T",
#line 34
#endif
#line 34
				   NULL, 0, N_("trace queries") },
#line 40 "smapccmd.opt"

#line 40
	{
#line 40
#ifdef HAVE_GETOPT_LONG
#line 40
	  "-p, --prompt",
#line 40
#else
#line 40
	  "-p",
#line 40
#endif
#line 40
				   N_("STRING"), 0, N_("set command prompt") },
#line 46 "smapccmd.opt"

#line 46
	{
#line 46
#ifdef HAVE_GETOPT_LONG
#line 46
	  "-B, --batch",
#line 46
#else
#line 46
	  "-B",
#line 46
#endif
#line 46
				   NULL, 0, N_("batch mode") },
#line 52 "smapccmd.opt"

#line 52
	{
#line 52
#ifdef HAVE_GETOPT_LONG
#line 52
	  "-q, --norc",
#line 52
#else
#line 52
	  "-q",
#line 52
#endif
#line 52
				   NULL, 0, N_("do not read user rc file") },
#line 58 "smapccmd.opt"

#line 58
	{
#line 58
#ifdef HAVE_GETOPT_LONG
#line 58
	  "-s, --source",
#line 58
#else
#line 58
	  "-s",
#line 58
#endif
#line 58
				   N_("ADDR"), 0, N_("set source address") },
#line 67 "smapccmd.opt"

#line 67
	{
#line 67
#ifdef HAVE_GETOPT_LONG
#line 67
	  "-S, --server",
#line 67
#else
#line 67
	  "-S",
#line 67
#endif
#line 67
				   N_("URL"), 0, N_("connect to this server") },
#line 73 "smapccmd.opt"

#line 73
	{
#line 73
#ifdef HAVE_GETOPT_LONG
#line 73
	  "-Q, --quiet",
#line 73
#else
#line 73
	  "-Q",
#line 73
#endif
#line 73
				   NULL, 0, N_("do not print the normal smapc welcome banner") },
#line 77 "smapccmd.opt"

#line 77
	{ NULL, NULL, 0, N_("Other options") },
#line 77 "smapccmd.opt"

#line 77
	{
#line 77
#ifdef HAVE_GETOPT_LONG
#line 77
	  "-h, --help",
#line 77
#else
#line 77
	  "-h",
#line 77
#endif
#line 77
				   NULL, 0, N_("Give this help list") },
#line 77 "smapccmd.opt"

#line 77
	{
#line 77
#ifdef HAVE_GETOPT_LONG
#line 77
	  "--usage",
#line 77
#else
#line 77
	  "",
#line 77
#endif
#line 77
				   NULL, 0, N_("Give a short usage message") },
#line 77 "smapccmd.opt"

#line 77
	{
#line 77
#ifdef HAVE_GETOPT_LONG
#line 77
	  "-V, --version",
#line 77
#else
#line 77
	  "-V",
#line 77
#endif
#line 77
				   NULL, 0, N_("Print program version") },

#line 77 "smapccmd.opt"
};
#line 17 "smapccmd.opt"

#line 17
const char *program_version = "smapc" " (" PACKAGE_NAME ") " PACKAGE_VERSION;
#line 17
static char doc[] = N_("A socket map client.");
#line 17
static char args_doc[] = N_("[URL] [map [key]]]");
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
  printf ("%s %s [%s]... %s\n", _("Usage:"), "smapc", _("OPTION"),
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
  n = snprintf (buf, sizeof buf, "%s %s ", _("Usage:"), "smapc");
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

#line 77 "smapccmd.opt"

#line 77


void
get_options(int argc, char *argv[], int *index)
{
    
#line 82
 {
#line 82
  int c;
#line 82

#line 82
#ifdef HAVE_GETOPT_LONG
#line 82
  while ((c = getopt_long(argc, argv, "d:x:Tp:Bqs:S:QhV",
#line 82
			  long_options, NULL)) != EOF)
#line 82
#else
#line 82
  while ((c = getopt(argc, argv, "d:x:Tp:Bqs:S:QhV")) != EOF)
#line 82
#endif
#line 82
    {
#line 82
      switch (c)
#line 82
	{
#line 82
	default:
#line 82
	   	   exit(EX_USAGE);
#line 82
	#line 27 "smapccmd.opt"
	 case 'd': case 'x':
#line 27
	  {
#line 27

       if (smap_debug_set(optarg))
           smap_error("invalid debug specification: %s", optarg);

#line 30
	     break;
#line 30
	  }
#line 34 "smapccmd.opt"
	 case 'T':
#line 34
	  {
#line 34

       smapc_trace_option = 1;

#line 36
	     break;
#line 36
	  }
#line 40 "smapccmd.opt"
	 case 'p':
#line 40
	  {
#line 40

       prompt = optarg;

#line 42
	     break;
#line 42
	  }
#line 46 "smapccmd.opt"
	 case 'B':
#line 46
	  {
#line 46

       batch_mode = 1;

#line 48
	     break;
#line 48
	  }
#line 52 "smapccmd.opt"
	 case 'q':
#line 52
	  {
#line 52

       norc = 1;

#line 54
	     break;
#line 54
	  }
#line 58 "smapccmd.opt"
	 case 's':
#line 58
	  {
#line 58

       if (inet_aton(optarg, &source_addr) == 0) {
            smap_error("invalid IP address: %s", optarg);
	    exit(EX_USAGE);
       }

#line 63
	     break;
#line 63
	  }
#line 67 "smapccmd.opt"
	 case 'S':
#line 67
	  {
#line 67

       server_option = optarg;

#line 69
	     break;
#line 69
	  }
#line 73 "smapccmd.opt"
	 case 'Q':
#line 73
	  {
#line 73

       quiet_startup = 1;

#line 75
	     break;
#line 75
	  }
#line 77 "smapccmd.opt"
	 case 'h':
#line 77
	  {
#line 77

#line 77
		print_help ();
#line 77
		exit (0);
#line 77
	 
#line 77
	     break;
#line 77
	  }
#line 77 "smapccmd.opt"
	 case OPTION_USAGE:
#line 77
	  {
#line 77

#line 77
		print_usage ();
#line 77
		exit (0);
#line 77
	 
#line 77
	     break;
#line 77
	  }
#line 77 "smapccmd.opt"
	 case 'V':
#line 77
	  {
#line 77

#line 77
		/* Give version */
#line 77
		print_version(program_version, stdout);
#line 77
		exit (0);
#line 77
	 
#line 77
	     break;
#line 77
	  }

#line 82 "smapccmd.opt"
	}
#line 82
    }
#line 82
  *index = optind;
#line 82
 }
#line 82

}
    

