/* This file is part of Smap.
   Copyright (C) 2008, 2010, 2014 Sergey Poznyakoff

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
#include <smap/parseopt.h>
#include <smap/diag.h>
#include <string.h>

static struct smap_option const *
find_opt(struct smap_option const *opt, const char *str, const char **value,
	 int flags)
{
	size_t len = strlen(str);
	int isbool;
	int delim = flags & SMAP_DELIM_MASK;
	
	if (len > 2 && (flags & SMAP_IGNORECASE
			? strncasecmp
			: strncmp)(str, "no", 2) == 0) {
		*value = NULL;
		str += 2;
		isbool = 1;
	} else {
		isbool = 0;
		*value = str;
	}
	
	for (; opt->name; opt++) {
		if (len >= opt->len
		    && (flags & SMAP_IGNORECASE
			? strncasecmp
			: strncmp)(opt->name, str, opt->len) == 0
		    && (!isbool || opt->type == smap_opt_bool)) {
			int eq;
			const char *vp;
			
			if (delim == SMAP_DELIM_EQ) {
				eq = str[opt->len] == '=';
				vp = str + opt->len + 1;
			} else if (delim == SMAP_DELIM_WS) {
				vp = str + opt->len;
				eq = isspace(*vp);
				if (eq) do ++vp; while (*vp && isspace(*vp));
			} else
				abort();
	    
			switch (opt->type) {
			case smap_opt_long:
			case smap_opt_string:
			case smap_opt_const_string:
			case smap_opt_enum:
				if (!eq)
					continue;
				*value = vp;
				break;
				
			case smap_opt_null:
				if (eq)
					*value = vp;
				else
					*value = NULL;
				break;					
				
			default:
				if (eq)
					continue;
				break;
			}
			return opt;
		}
	}
	return NULL;
}

static int
find_value(const char **enumstr, const char *value)
{
	int i;
	for (i = 0; *enumstr; enumstr++, i++)
		if (strcmp(*enumstr, value) == 0)
			return i;
	return -1;
}

int
smap_parseline(struct smap_option const *opt, const char *line, int flags,
	       char **errmsg) 
{
	int rc = SMAP_PARSE_SUCCESS;
	long n;
	char *s;
	const char *value;
	struct smap_option const *p = find_opt(opt, line, &value, flags);

	if (!p)
		return SMAP_PARSE_NOENT;

	switch (p->type) {
	case smap_opt_long:
		n = strtol(value, &s, 0);
		if (*s) {
			*errmsg = "not a valid number";
			rc = SMAP_PARSE_INVAL;
			break;
		}
		*(long*)p->data = n;
		break;
	    
	case smap_opt_const:
		*(long*)p->data = p->v.value;
		break;
	    
	case smap_opt_const_string:
		*(const char**)p->data = value;
		break;
	    
	case smap_opt_string:
		*(const char**)p->data = strdup(value);
		break;
	    
	case smap_opt_bool:
		if (p->v.value) {
			if (value)
				*(int*)p->data |= p->v.value;
			else
				*(int*)p->data &= ~p->v.value;
		} else
			*(int*)p->data = value != NULL;
		break;
			
	case smap_opt_bitmask:
		*(int*)p->data |= p->v.value;
		break;
			
	case smap_opt_bitmask_rev:
		*(int*)p->data &= ~p->v.value;
		break;
	    
	case smap_opt_enum:
		n = find_value(p->v.enumstr, value);
		if (n == -1) {
			*errmsg = "invalid value";
			rc = SMAP_PARSE_INVAL;
			break;
		}
		*(int*)p->data = n;
		break;
		
	case smap_opt_null:
		break;
	}
	
	if (p->func && p->func(p, value, errmsg))
		rc = SMAP_PARSE_INVAL;
	return rc;
}		

int
smap_parseopt(struct smap_option const *opt, int argc, char **argv, int flags,
	      int *pindex)
{
	int i;
	long n;
	char *s;
	int rc = 0;
	const char *modname = argv[0];
	
	for (i = (flags & SMAP_PARSEOPT_PARSE_ARGV0) ? 0 : 1;
	     i < argc; i++) {
		char *errmsg;
		int res;

		res = smap_parseline(opt, argv[i], SMAP_DELIM_EQ, &errmsg);

		if (res == SMAP_PARSE_SUCCESS)
			continue;
		else if (res == SMAP_PARSE_NOENT) {
			if (pindex) {
				if (flags & SMAP_PARSEOPT_PERMUTE) {
					int j;
					struct smap_option const *p = NULL;
					const char *value;

					for (j = i + 1; j < argc; j++) 
						if ((p = find_opt(opt,
								  argv[j],
								  &value,
							     SMAP_DELIM_EQ)))
							break;
					
					if (p) {
						char *tmp = argv[j];
						argv[j] = argv[i];
						argv[i] = tmp;
						--i;
					} else
						break;
				} else
					break;
			} else {
				smap_error("%s: %s: unknown option",
					   modname, argv[i]);
				rc = 1;
			}
		} else if (res == SMAP_PARSE_INVAL) {
			smap_error("%s: %s: %s",
				   modname, argv[i], errmsg);
			rc = 1;
		}
	}
	if (pindex)
		*pindex = i;
	return rc;
}
