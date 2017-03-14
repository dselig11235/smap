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

#include "smapd.h"

char *mod_load_path[2];

void
add_load_path(const char *path, int pathid)
{
	if (path[0] == ':')
		path++;
	if (mod_load_path[pathid]) {
		mod_load_path[pathid] = erealloc(mod_load_path[pathid],
						 strlen(mod_load_path[pathid]) +
						 strlen(path) + 2);
		strcat(mod_load_path[pathid], ":");
		strcat(mod_load_path[pathid], path);
	} else
		mod_load_path[pathid] = estrdup(path);
}

static void
flush_load_path(const char *path)
{
	int i;
	struct wordsplit ws;
	ws.ws_delim = ":";
	ws.ws_error = smap_error;

	if (!path)
		return;
	if (wordsplit(path, &ws,
		      WRDSF_NOCMD | WRDSF_NOVAR |
		      WRDSF_ENOMEMABRT | WRDSF_SQUEEZE_DELIMS |
		      WRDSF_DELIM | WRDSF_ERROR)) {
		smap_error("cannot parse load path: %s",
			   wordsplit_strerror(&ws));
		return;
	}

	for (i = 0; i < ws.ws_wordc; i++) {
		if (ws.ws_wordv[i][0]) {
			debug(DBG_MODULE, 1,
			      ("adding %s to the load path",
			       ws.ws_wordv[i]));
			if (lt_dladdsearchdir(ws.ws_wordv[i])) {
				smap_error("cannot add `%s' to "
					   "the load path: %s",
					   ws.ws_wordv[i],
					   lt_dlerror());
			}
		}
	}
	wordsplit_free(&ws);
}

void
smap_modules_init()
{
	lt_dlinit();

	flush_load_path(mod_load_path[PATH_PREPEND]);
	
	debug(DBG_MODULE, 1,
	      ("adding %s to the load path", SMAP_MODDIR));
	lt_dladdsearchdir(SMAP_MODDIR);

	flush_load_path(mod_load_path[PATH_APPEND]);
}



DCLIDLIST(module, smap_module_instance)

void remove_dependent_databases(const char *id);

int
module_declare(const char *file, unsigned line,
	       const char *id, int argc, char **argv,
	       struct smap_module_instance **pmod)
{
	int i;
	struct smap_module_instance *mip;

	mip = module_locate(id);
	if (mip) {
		*pmod = mip;
		return 1;
	}

	mip = emalloc(sizeof(*mip));
	mip->file = estrdup(file);
	mip->line = line;
	mip->id = estrdup(id);
	mip->argc = argc;
	mip->argv = ecalloc(argc + 1, sizeof(mip->argv[0]));
	for (i = 0; i < argc; i++)
		mip->argv[i] = estrdup(argv[i]);
	mip->argv[i] = NULL;
	mip->module = NULL;
	mip->handle = NULL;
	module_attach(mip);
	*pmod = mip;
	return 0;
}

#define MODULE_ASSERT(cond)						\
	do {								\
		if (!(cond)) {						\
			lt_dlclose(handle);				\
			smap_error("%s: faulty module: (%s) failed",	\
				   inst->id,				\
				   #cond);				\
			return 1;					\
		}							\
	} while (0)

static int
_load_module(struct smap_module_instance *inst)
{
	lt_dlhandle handle = NULL;
	lt_dladvise advise = NULL;
	struct smap_module *pmod;

	debug(DBG_MODULE, 2, ("loading module %s", inst->id));

	if (inst->handle) {
		smap_error("module %s already loaded", inst->id);
		return 1;
	}

	if (!lt_dladvise_init(&advise) && !lt_dladvise_ext(&advise)
	    && !lt_dladvise_global(&advise))
		handle = lt_dlopenadvise(inst->argv[0], advise);
	lt_dladvise_destroy(&advise);

	if (!handle) {
		smap_error("cannot load module %s: %s", inst->id,
			   lt_dlerror());
		return 1;
	}

	pmod = (struct smap_module *) lt_dlsym(handle, "module");
	MODULE_ASSERT(pmod);
	MODULE_ASSERT(pmod->smap_version <= SMAP_MODULE_VERSION);
	MODULE_ASSERT(pmod->smap_init_db);
	MODULE_ASSERT(pmod->smap_free_db);
	if (pmod->smap_version == 1)
	    	MODULE_ASSERT(pmod->smap_query);
	else {
		if (pmod->smap_capabilities & SMAP_CAPA_QUERY)
			MODULE_ASSERT(pmod->smap_query);
		if (pmod->smap_capabilities & SMAP_CAPA_XFORM)
			MODULE_ASSERT(pmod->smap_xform);
	}
	
	if (pmod->smap_init && pmod->smap_init(inst->argc, inst->argv)) {
		lt_dlclose(handle);
		smap_error("%s: initialization failed", inst->argv[0]);
		return 1;
	}
	inst->handle = handle;
	inst->module = pmod;
	return 0;
}

void
module_instance_free(struct smap_module_instance *inst)
{
	int i;
	free(inst->file);
	free(inst->id);
	for (i = 0; i < inst->argc; i++)
		free(inst->argv[i]);
	free(inst->argv);
	/* FIXME: Handle */
	free(inst);
}

void
smap_modules_load()
{
	struct smap_module_instance *p;

	debug(DBG_MODULE, 1, ("loading modules"));
	for (p = module_head; p; ) {
		struct smap_module_instance *next = p->next;

		if (_load_module(p)) {
			remove_dependent_databases(p->id);
			debug(DBG_MODULE, 2, ("removing module %s", p->id));
			module_detach(p);
			module_instance_free(p);
		}
		p = next;
	}
}

void
smap_modules_unload()
{
	struct smap_module_instance *p;

	for (p = module_head; p; p = p->next) {
		debug(DBG_MODULE, 2, ("unloading module %s", p->id));
		lt_dlclose(p->handle);
		p->handle = NULL;
	}
}

DCLIDLIST(database, smap_database_instance);

int
database_declare(const char *file, unsigned line,
		 const char *id, const char *modname, int argc, char **argv,
		 struct smap_database_instance **pdb)
{
	int i;
	struct smap_database_instance *db;

	db = database_locate(id);
	if (db) {
		*pdb = db;
		return 1;
	}
	db = ecalloc(1, sizeof(*db));
	db->file = estrdup(file);
	db->line = line;
	db->id = estrdup(id);
	db->modname = estrdup(modname);
	db->argc = argc;
	db->argv = ecalloc(argc + 1, sizeof(db->argv[0]));
	for (i = 0; i < argc; i++)
		db->argv[i] = estrdup(argv[i]);
	db->argv[i] = NULL;
	db->inst = NULL;
	database_attach(db);
	*pdb = db;
	return 0;
}

void
database_free(struct smap_database_instance *db)
{
	int i;
	free(db->file);
	free(db->id);
	free(db->modname);
	for (i = 0; i < db->argc; i++)
		free(db->argv[i]);
	free(db->argv);
	free(db);
}

void
remove_dependent_databases(const char *modname)
{
	struct smap_database_instance *p;

	for (p = database_head; p; ) {
		struct smap_database_instance *next = p->next;
		if (strcmp(p->modname, modname) == 0) {
			debug(DBG_MODULE, 1, ("removing database %s", p->id));
			database_detach(p);
			database_free(p);
		}
		p = next;
	}
}

void
init_databases()
{
	struct smap_database_instance *p;

	debug(DBG_DATABASE, 1, ("initializing databases"));
	for (p = database_head; p; ) {
		struct smap_database_instance *next = p->next;
		struct smap_module_instance *inst = module_locate(p->modname);

		if (!inst)
			smap_error("%s:%u: module %s is not declared",
				   p->file, p->line, p->modname);
		else {
			debug(DBG_DATABASE, 2, ("initializing database %s",
						inst->id));
			
			p->dbh = inst->module->smap_init_db(p->id,
				                            p->argc,
							    p->argv);
			if (!p->dbh)
				smap_error("%s:%u: module %s: "
					   "database initialization failed",
					   p->file, p->line, p->modname);
		}
		if (!p->dbh) {
			debug(DBG_DATABASE, 2,
			      ("removing database %s", p->id));
			database_detach(p);
			database_free(p);
		}
		p->inst = inst;
		p = next;
	}
}

void
close_databases()
{
	struct smap_database_instance *p;

	debug(DBG_DATABASE, 1, ("closing databases"));
	for (p = database_head; p; p = p->next) {
		if (p->opened) {
			debug(DBG_DATABASE, 2,
			      ("closing database %s", p->id));
			if (p->inst->module->smap_close)
				p->inst->module->smap_close(p->dbh);
			p->opened = 0;
		}
	}
}

void
free_databases()
{
	struct smap_database_instance *p;

	debug(DBG_DATABASE, 1, ("freeing databases"));
	for (p = database_head; p; p = p->next) {
		struct smap_module *mod = p->inst->module;
		debug(DBG_DATABASE, 2,
		      ("freeing database %s", p->id));
		if (mod->smap_free_db)
			mod->smap_free_db(p->dbh);
		p->dbh = NULL;
	}
}
