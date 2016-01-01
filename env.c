/*
  +----------------------------------------------------------------------+
  | PHP Version 5                                                        |
  +----------------------------------------------------------------------+
  | Copyright (c) 1997-2015 The PHP Group                                |
  +----------------------------------------------------------------------+
  | This source file is subject to version 3.01 of the PHP license,      |
  | that is bundled with this package in the file LICENSE, and is        |
  | available through the world-wide-web at the following url:           |
  | http://www.php.net/license/3_01.txt                                  |
  | If you did not receive a copy of the PHP license and are unable to   |
  | obtain it through the world-wide-web, please send a note to          |
  | license@php.net so we can mail you a copy immediately.               |
  +----------------------------------------------------------------------+
  | Author:                                                              |
  +----------------------------------------------------------------------+
*/

#include <stddef.h>
#include <stdio.h>
#include <string.h>
#include <dlfcn.h>
#include "php_env.h"
#include "env.h"

typedef void (*ts_allocate_ctor)(void *);
typedef void (*ts_allocate_dtor)(void *);
int (*my_ts_allocate_id)(int *rsrc_id, size_t size, ts_allocate_ctor ctor, ts_allocate_dtor dtor) = NULL;
void *(*my_ts_resource_ex)(int id, void *th_id) = NULL;

static void* ini_entries = NULL;
static int (*php_env_module_init)();

int env_globals_id;
zend_env_globals env_globals;

#define PHP_INI_SYSTEM (1<<2)
extern int OnUpdateString();

/* {{{ PHP 5 INI */
struct php5_ini_entry {
	int module_number;
	int modifiable;
	char *name;
	unsigned int name_length;
	void *on_modify;
	void *mh_arg1;
	void *mh_arg2;
	void *mh_arg3;

	char *value;
	unsigned int value_length;

	char *orig_value;
	unsigned int orig_value_length;
	int orig_modifiable;
	int modified;

	void (*displayer)(void *ini_entry, int type);
};
static struct php5_ini_entry ini_entries5[] = {
	{0, PHP_INI_SYSTEM, "env.file", sizeof("env.file"), OnUpdateString, (void *) offsetof(zend_env_globals, file), (void *) &env_globals, NULL, NULL, 0, NULL, 0, 0, 0, NULL },
	{0, 0, NULL, 0, NULL, NULL, NULL, NULL, NULL, 0, NULL, 0, 0, 0, NULL}
};
/* }}} */

/* {{{ PHP 7 INI */
struct php7_ini_entry {
	const char *name;
	void *on_modify;
	void *mh_arg1;
	void *mh_arg2;
	void *mh_arg3;
	const char *value;
	void (*displayer)(void *ini_entry, int type);
	int modifiable;

	unsigned int name_length;
	unsigned int value_length;
};
static struct php7_ini_entry ini_entries7[] = {
	{ "env.file", OnUpdateString, (void*)offsetof(zend_env_globals, file), (void*)&env_globals, NULL, NULL, NULL, PHP_INI_SYSTEM, sizeof("env.file")-1, 0},
	{ NULL, NULL, NULL, NULL, NULL, NULL, NULL, 0, 0, 0}
};
/* }}} */

static void php_env_init_globals(zend_env_globals *env_globals) /* {{{ */
{
	env_globals->file = NULL;
	env_globals->parse_err = 0;
}
/* }}} */

static void php_env_shutdown_globals(zend_env_globals *env_globals) /* {{{ */
{
	env_globals->file = NULL;
	env_globals->parse_err = 0;
}
/* }}} */

int zend_register_ini_entries(void *ini_entry, int module_number, void *tsrm);
void zend_unregister_ini_entries(int module_number, void *tsrm);
void display_ini_entries(int zend_module);

/* {{{ PHP_MINIT_FUNCTION */
int zm_startup_env(int type, int module_number, void *tsrm)
{
	if (my_ts_allocate_id) {
		my_ts_allocate_id(&env_globals_id, sizeof(zend_env_globals), (ts_allocate_ctor)php_env_init_globals, NULL);//(ts_allocate_dtor)php_env_shutdown_globals);
	} else {
		php_env_init_globals(&env_globals);
	}

	zend_register_ini_entries((void *)ini_entries, module_number, tsrm);
	return php_env_module_init(tsrm);
}
/* }}} */

/* {{{ PHP_MSHUTDOWN_FUNCTION
 */
int zm_shutdown_env(int type, int module_number, void *tsrm)
{
	zend_unregister_ini_entries(module_number, tsrm);

	return 0; /* SUCCESS */
}
/* }}} */

void php_info_print_table_start(void);
void php_info_print_table_end(void);
void php_info_print_table_header(int num_cols, ...);

/* {{{ PHP_MINFO_FUNCTION */
void zm_info_env(void *zend_module)
{
	php_info_print_table_start();
	php_info_print_table_header(2, "env support", "enabled");
	php_info_print_table_end();

	//display_ini_entries(zend_module);
}
/* }}} */

static char build_id[50];

/* {{{ env_module_entry
 */
typedef struct _zend_module_entry {
	unsigned short size;
	unsigned int zend_api;
	unsigned char zend_debug;
	unsigned char zts;
	const struct _zend_ini_entry *ini_entry;
	const struct _zend_module_dep *deps;
	const char *name;
	const struct _zend_function_entry *functions;
	int (*module_startup_func)(int type, int module_number, void *tsrm);
	int (*module_shutdown_func)(int type, int module_number, void *tsrm);
	void *request_startup_func;
	void *request_shutdown_func;
	void (*info_func)(void *zend_module);
	const char *version;
	size_t globals_size;
	void *globals_ptr;
	void *globals_ctor;
	void *globals_dtor;
	int (*post_deactivate_func)(void);
	int module_started;
	unsigned char type;
	void *handle;
	int module_number;
	const char *build_id;
} zend_module_entry;

zend_module_entry env_module_entry = {
	sizeof(zend_module_entry),
	0,
	0,
	0,
	NULL,
	NULL,
	"env",
	NULL,
	zm_startup_env,
	zm_shutdown_env,
	NULL,
	NULL,
	zm_info_env,
	PHP_ENV_VERSION,
	0,
	NULL,
	NULL,
	NULL,
	NULL,
	0,
	0,
	NULL,
	0,
	build_id
};
/* }}} */


static void get_build_id(int api) {
	void *debug_symbol = NULL;
	void *self = dlopen(NULL, RTLD_NOW);
	if (self) {
		debug_symbol = dlsym(self, "_zval_ptr_dtor_wrapper");
		my_ts_allocate_id = dlsym(self, "ts_allocate_id");
		my_ts_resource_ex = dlsym(self, "ts_resource_ex");
		dlclose(self);
	}
	snprintf(build_id, sizeof(build_id), "API%d,%s%s", api, my_ts_allocate_id ? "TS" : "NTS", debug_symbol ? ",debug" : "");
}

char *zend_get_module_version(char *);

zend_module_entry *get_module(void) { /* {{{ */
	const char *zend_version_string = zend_get_module_version("standard");

	/* we could use version compare here .... */
	if (zend_version_string && zend_version_string[0] == '5') {
		if (strstr(zend_version_string, "5.5.") == zend_version_string) {
			env_module_entry.zend_api = 20121212;
		} else if (strstr(zend_version_string, "5.6.") == zend_version_string) {
			env_module_entry.zend_api = 20131226;
		}
		ini_entries = ini_entries5;
		php_env_module_init = php_env_module_init5;
	} else if (zend_version_string && zend_version_string[0] == '7') {
		if (strstr(zend_version_string, "7.0.") == zend_version_string) {
			env_module_entry.zend_api = 20151012;
		} else if (strstr(zend_version_string, "7.1.") == zend_version_string) {
			env_module_entry.zend_api = 20151012; // Not changed since 7.0, yet?
		}
		ini_entries = ini_entries7;
		php_env_module_init = php_env_module_init7;
	}

	get_build_id(env_module_entry.zend_api);

	return &env_module_entry;
}
/* }}} */

/*
 * Local variables:
 * tab-width: 4
 * c-basic-offset: 4
 * End:
 * vim600: noet sw=4 ts=4 fdm=marker
 * vim<600: noet sw=4 ts=4
 */
