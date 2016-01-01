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

/* $Id$ */

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include "php.h"
#include "php_ini.h"
#include "ext/standard/info.h"
#include "php_env.h"
#include "env.h"

static void* ini_entries = NULL;
static int (*php_env_module_init)();

ZEND_DECLARE_MODULE_GLOBALS(env)

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

	void (*displayer)(zend_ini_entry *ini_entry, int type);
};
static struct php5_ini_entry ini_entries5[] = {
	{0, PHP_INI_ALL, "env.file", sizeof("env.file"), OnUpdateString, (void *) XtOffsetOf(zend_env_globals, file), (void *) &env_globals, NULL, NULL, 0, NULL, 0, 0, 0, NULL },
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
	void (*displayer)(zend_ini_entry *ini_entry, int type);
	int modifiable;

	unsigned int name_length;
	unsigned int value_length;
};
static struct php7_ini_entry ini_entries7[] = {
	{ "env.file", OnUpdateString, (void*)XtOffsetOf(zend_env_globals, file), (void*)&env_globals, NULL, NULL, NULL, PHP_INI_ALL, sizeof("env.file")-1, 0},
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

/* {{{ PHP_MINIT_FUNCTION
 */
int zm_startup_env(int type, int module_number)
{
	ZEND_INIT_MODULE_GLOBALS(env, php_env_init_globals, php_env_shutdown_globals);

	zend_register_ini_entries((void *)ini_entries, module_number);
	return php_env_module_init();
}
/* }}} */

/* {{{ PHP_MSHUTDOWN_FUNCTION
 */
int zm_shutdown_env(int type, int module_number)
{
	UNREGISTER_INI_ENTRIES();

	return SUCCESS;
}
/* }}} */


/* {{{ PHP_MINFO_FUNCTION
 */
void zm_info_env(zend_module_entry *zend_module)
{
	php_info_print_table_start();
	php_info_print_table_header(2, "env support", "enabled");
	php_info_print_table_end();

	DISPLAY_INI_ENTRIES();
}
/* }}} */

/* {{{ env_module_entry
 */
zend_module_entry env_module_entry = {
	STANDARD_MODULE_HEADER,
	"env",
	NULL,
	PHP_MINIT(env),
	PHP_MSHUTDOWN(env),
	NULL,
	NULL,
	PHP_MINFO(env),
	PHP_ENV_VERSION,
	STANDARD_MODULE_PROPERTIES
};
/* }}} */

#if ZEND_DEBUG
#define BUILD_DEBUG_STRING ",debug"
#else
#define BUILD_DEBUG_STRING
#endif

static char build_id[50];

static int is_debug_build() {
	void *self = dlopen(NULL, RTLD_NOW);
	if (self) {
		void *symbol = dlsym(self, "_zval_ptr_dtor_wrapper");
		dlclose(self);
		return symbol != NULL;
	}
	return 0;
}

static char* get_build_id(int api) {
	snprintf(build_id, sizeof(build_id), "API%d,NTS%s", api, is_debug_build() ? ",debug" : "");
	return build_id;
}

zend_module_entry *get_module(void) { /* {{{ */
	const char *zend_version_string = zend_get_module_version("standard");

	/* we could use version compare here .... */
	if (zend_version_string && strstr(zend_version_string, "5.5.") == zend_version_string) {
		env_module_entry.zend_api = 20121212;
		env_module_entry.build_id = get_build_id(env_module_entry.zend_api);
		ini_entries = ini_entries5;
		php_env_module_init = php_env_module_init5;
	} else if (zend_version_string && strstr(zend_version_string, "5.6.") == zend_version_string) {
		env_module_entry.zend_api = 20131226;
		env_module_entry.build_id = get_build_id(env_module_entry.zend_api);
		ini_entries = ini_entries5;
		php_env_module_init = php_env_module_init5;
	} else if (zend_version_string && strstr(zend_version_string, "7.0.") == zend_version_string) {
		env_module_entry.zend_api = 20151012;
		env_module_entry.build_id = get_build_id(env_module_entry.zend_api);
		ini_entries = ini_entries7;
		php_env_module_init = php_env_module_init7;
	} else if (zend_version_string && strstr(zend_version_string, "7.1.") == zend_version_string) {
		env_module_entry.zend_api = 20151012; // Not chaned since 7.0, yet?
		env_module_entry.build_id = get_build_id(env_module_entry.zend_api);
		ini_entries = ini_entries7;
		php_env_module_init = php_env_module_init7;
	} else {
		env_module_entry.zend_api = 0;
		env_module_entry.build_id = "UNSUPPOTED VERSION DETECTED";
	}

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
