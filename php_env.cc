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

#include <stdlib.h>
#include <stdio.h>
#include <string.h>

#include "php_env.h"

extern void *my_ts_allocate_id;
extern void *(*my_ts_resource_ex)(int id, void *th_id);

extern int env_globals_id;
extern zend_env_globals env_globals;

zend_env_globals *G(void *tsrm) {
	if (my_ts_allocate_id) {
		return (zend_env_globals*) (*((void ***) tsrm))[env_globals_id-1];
	} else {
		return &env_globals;
	}
};

/* {{{ PHP APIs */
#define VCWD_FOPEN(path, mode)  fopen(path, mode)

typedef unsigned char zend_bool;

extern "C" void zend_error(int type, const char *format, ...);

typedef enum {
	ZEND_HANDLE_FILENAME,
	ZEND_HANDLE_FD,
	ZEND_HANDLE_FP,
	ZEND_HANDLE_STREAM,
	ZEND_HANDLE_MAPPED
} zend_stream_type;

struct zend_stream {
	void      *handle;
	int       isatty;
	struct {
		size_t len;
		size_t pos;
		void   *map;
		char   *buf;
		void   *old_handle;
		void   *old_closer;
	} mmap;
	void      *reader;
	void      *fsizer;
	void      *closer;
};

struct zend_file_handle5 {
	zend_stream_type  type;
	char              *filename;
	char              *opened_path;
	union {
		int           fd;
		FILE          *fp;
		zend_stream   stream;
	} handle;
	zend_bool free_filename;
};

struct zend_file_handle7 {
	union {
		int           fd;
		FILE          *fp;
		zend_stream   stream;
	} handle;
	const char        *filename;
	void              *opened_path;
	zend_stream_type  type;
	zend_bool free_filename;
};


#define ZEND_INI_PARSER_ENTRY     1 /* Normal entry: foo = bar */
#define ZEND_INI_PARSER_SECTION   2 /* Section: [foobar] */
#define ZEND_INI_PARSER_POP_ENTRY 3 /* Offset entry: foo[] = bar */

#define ENV_G(v) (G(tsrm)->v)

typedef void (*zend_ini_parser_cb_t)(void *arg1, void *arg2, void *arg3, int callback_type, void *arg);
extern "C" int zend_parse_ini_file(void *fh, zend_bool unbuffered_errors, int scanner_mode, zend_ini_parser_cb_t ini_parser_cb, void *arg);

/* }}} */

/* {{{ PHP 5 zval */
struct zval5 {
	union _zvalue_value {
		long lval;                  /* long value */
		double dval;                /* double value */
		struct {
			char *val;
			int len;
		} str;
		void *ht;              /* hash table value */
		struct {
			void *ce;
			void *properties;
			void *guards; /* protects from __get/__set ... recursion */
		} zend_object;
	} value;
	unsigned int refcount__gc;
	unsigned char type;    /* active type */
	unsigned char is_ref__gc;
};
char *Z_STRVAL_P(zval5 *zv) {
	return zv->value.str.val;
}
/* }}} */

/* {{{ PHP 7 zval */
//#if (sizeof(int) == 4)
typedef unsigned int php_uint32_t;
//# elif (sizeof(long) == 4)
//typedef unsigned long int php_uint32_t;
//# endif

struct php7_string {
	struct {
		php_uint32_t refcount;
		php_uint32_t type_info;
	} gc;
	unsigned long h;
	size_t len;
	char val[1];
};

struct zval7 {
	// in PHP 7 the str pointer will always be at offset 0, so we ignore the other things
	struct php7_string *string;
};
char *Z_STRVAL_P(zval7 *zv) {
	return zv->string->val;
}
/* }}} */



namespace {

template<typename zval_t>
void php_env_ini_parser_cb(zval_t *key, zval_t *value, zval_t *index, int callback_type, void *arg) {
	void *tsrm = NULL;

	if (my_ts_allocate_id) {
		tsrm = (void *)my_ts_resource_ex(0, NULL);
	}

	if (ENV_G(parse_err)) {
		return;
	}

	if (value == NULL) {
		return;
	}

	if (callback_type == ZEND_INI_PARSER_ENTRY) {
		setenv(Z_STRVAL_P(key), Z_STRVAL_P(value), 1);
	} else if (callback_type == ZEND_INI_PARSER_SECTION || callback_type == ZEND_INI_PARSER_POP_ENTRY) {
		ENV_G(parse_err) = 1;
	}
}

typedef int uint32_t;

template<typename zval_t, zend_ini_parser_cb_t cb, typename zend_file_handle>
int php_env_module_init(void *tsrm) {
	int ndir = 255;
	uint32_t i;
	unsigned char c;
	zend_file_handle fh;

	if (ENV_G(file) != NULL && strlen(ENV_G(file)) > 0) {
		if ((fh.handle.fp = VCWD_FOPEN(ENV_G(file), "r"))) {
			fh.filename = ENV_G(file);
			fh.type = ZEND_HANDLE_FP;
			fh.free_filename = 0;
			fh.opened_path = NULL;

			if (zend_parse_ini_file(&fh, 0, 0 /* ZEND_INI_SCANNER_NORMAL */, cb, NULL) == -1 || ENV_G(parse_err)) {
				if (ENV_G(parse_err)) {
					zend_error(2 /* E_WARNING */, "env: parsing '%s' failed", ENV_G(file));
				}

				ENV_G(parse_err) = 0;

				return 0;
			}
		}
	}

	return 0;
}
}

extern "C" {
void php_env_ini_parser_cb55(void *key, void *value, void *index, int callback_type, void *arg) {
	php_env_ini_parser_cb((zval5*)key, (zval5*)value, (zval5*)index, callback_type, arg);
}

void php_env_ini_parser_cb71(void *key, void *value, void *index, int callback_type, void *arg) {
	php_env_ini_parser_cb((zval7*)key, (zval7*)value, (zval7*)index, callback_type, arg);
}

int php_env_module_init5(void *tsrm) {
	return php_env_module_init<zval5, php_env_ini_parser_cb55, zend_file_handle5>(tsrm);
}

int php_env_module_init7(void *tsrm) {
	return php_env_module_init<zval7, php_env_ini_parser_cb71, zend_file_handle7>(tsrm);
}
}

/*
 * Local variables:
 * tab-width: 4
 * c-basic-offset: 4
 * End:
 * vim600: noet sw=4 ts=4 fdm=marker
 * vim<600: noet sw=4 ts=4
 */
