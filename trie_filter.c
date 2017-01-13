/*
  +----------------------------------------------------------------------+
  | PHP Version 5                                                        |
  +----------------------------------------------------------------------+
  | Copyright (c) 1997-2010 The PHP Group                                |
  +----------------------------------------------------------------------+
  | This source file is subject to version 3.01 of the PHP license,      |
  | that is bundled with this package in the file LICENSE, and is        |
  | available through the world-wide-web at the following url:           |
  | http://www.php.net/license/3_01.txt                                  |
  | If you did not receive a copy of the PHP license and are unable to   |
  | obtain it through the world-wide-web, please send a note to          |
  | license@php.net so we can mail you a copy immediately.               |
  +----------------------------------------------------------------------+
  | Author:  Lijun Wu    <wulijun01234@gmail.com>                           |
  +----------------------------------------------------------------------+
*/

/* $Id$ */

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include "php.h"
#include "php_ini.h"
#include "ext/standard/info.h"
#include "php_trie_filter.h"
//#include <stdio.h>

ZEND_DECLARE_MODULE_GLOBALS(trie_filter);

/* {{{ trie_filter_functions[]
 *
 * Every user visible function must have an entry in trie_filter_functions[].
 */
zend_function_entry trie_filter_functions[] = {
	PHP_FE(trie_filter_search, NULL)
    PHP_FE(trie_filter_init, NULL)
	{NULL, NULL, NULL}	/* Must be the last line in trie_filter_functions[] */
};
/* }}} */

/* {{{ trie_filter_module_entry
 */
zend_module_entry trie_filter_module_entry = {
#if ZEND_MODULE_API_NO >= 20010901
	STANDARD_MODULE_HEADER,
#endif
	"trie_filter",
	trie_filter_functions,
	PHP_MINIT(trie_filter),
	PHP_MSHUTDOWN(trie_filter),
	NULL,
	PHP_RSHUTDOWN(trie_filter),
	PHP_MINFO(trie_filter),
#if ZEND_MODULE_API_NO >= 20010901
	"0.1", /* Replace with version number for your extension */
#endif
	STANDARD_MODULE_PROPERTIES
};
/* }}} */

#ifdef COMPILE_DL_TRIE_FILTER
ZEND_GET_MODULE(trie_filter)
#endif

/* {{{ PHP_INI
 */
/*
PHP_INI_BEGIN()
    PHP_INI_ENTRY("trie_filter.dict_charset", "utf-8", PHP_INI_ALL, NULL)
PHP_INI_END()
*/
/* }}} */

static void php_trie_filter_dtor(zend_resource *rsrc TSRMLS_DC)
{
	Trie *trie = (Trie *)rsrc->ptr;
	trie_free(trie);
}

/* {{{ PHP_GINIT_FUNCTION
*/
PHP_GINIT_FUNCTION(trie_filter)
{
	trie_filter_globals->initNum = 0;
    trie_filter_globals->newNum = 0;
    trie_filter_globals->pTrie = NULL;
}
/* }}} */

/* {{{ PHP_MINIT_FUNCTION
 */
PHP_MINIT_FUNCTION(trie_filter)
{	
	return SUCCESS;
}
/* }}} */

/* {{{ PHP_MSHUTDOWN_FUNCTION
 */
PHP_MSHUTDOWN_FUNCTION(trie_filter)
{
    if (TRIE_FILTER_G(pTrie) != NULL) {
        trie_free(TRIE_FILTER_G(pTrie));
    }
    
	return SUCCESS;
}
/* }}} */

/* {{{ PHP_MSHUTDOWN_FUNCTION
 */
PHP_RSHUTDOWN_FUNCTION(trie_filter)
{
	return SUCCESS;    
}
/* }}} */

/* {{{ PHP_MINFO_FUNCTION
 */
PHP_MINFO_FUNCTION(trie_filter)
{
	php_info_print_table_start();
	php_info_print_table_header(2, "trie_filter support", "enabled");
	php_info_print_table_end();
}
/* }}} */

static int trie_search_one(Trie *trie, const AlphaChar *text, int *offset, TrieData *length)
{
	TrieState *s;
	const AlphaChar *p;
	const AlphaChar *base;

	base = text;
    if (! (s = trie_root(trie))) {
        return -1;
    }

	while (*text) {
		p = text;
		if (! trie_state_is_walkable(s, *p)) {
            trie_state_rewind(s);
			text++;
			continue;
		} else {
			trie_state_walk(s, *p++);
        }

		while (trie_state_is_walkable(s, *p) && ! trie_state_is_terminal(s))
			trie_state_walk(s, *p++);

		if (trie_state_is_terminal(s)) {
			*offset = text - base;
			*length = p - text;
            trie_state_free(s);

			return 1;
		}

        trie_state_rewind(s);
		text++;
	}
    trie_state_free(s);

	return 0;
}

/* {{{ proto array trie_filter_search(string centent)
   Returns info about first keyword, or false on error*/
PHP_FUNCTION(trie_filter_search)
{
	zend_string *text;

	int offset = -1, i, ret;
    TrieData length = 0;

	AlphaChar *alpha_text;

	if (zend_parse_parameters(ZEND_NUM_ARGS() TSRMLS_CC, "S", &text) == FAILURE) {
		RETURN_FALSE;
	}

    array_init(return_value);
    if (text->len < 1) {
		php_error_docref(NULL TSRMLS_CC, E_NOTICE, "input is empty");
		return;
	}

	alpha_text = emalloc(sizeof(AlphaChar) * ((text->len) + 1));
	for (i = 0; i < text->len; i++) {
		alpha_text[i] = (AlphaChar) ((unsigned char *) text->val)[i];
	}

	alpha_text[text->len] = TRIE_CHAR_TERM;

	ret = trie_search_one(TRIE_FILTER_G(pTrie), alpha_text, &offset, &length);
    efree(alpha_text);
	if (ret == 0) {
        return;
    } else if (ret == 1) {
		add_next_index_long(return_value, offset);
		add_next_index_long(return_value, length);
	} else {
        RETURN_FALSE;
    }
}
/* }}} */

/* {{{ proto resource trie_filter_init(string dict_file_path)
   Returns resource id, or FALSE on error*/
PHP_FUNCTION(trie_filter_init)
{
    zend_string *path;
    char separator[32] = { 0 };
    
    ++ TRIE_FILTER_G(initNum);
    if(TRIE_FILTER_G(pTrie) == NULL) {
        ++ TRIE_FILTER_G(newNum);
        
        if (zend_parse_parameters(ZEND_NUM_ARGS() TSRMLS_CC, "S", &path) == FAILURE) {
            RETURN_NULL();
        }
        
        TRIE_FILTER_G(pTrie) = trie_new_from_file(path->val);
        if (!TRIE_FILTER_G(pTrie)) {
            php_error_docref(NULL TSRMLS_CC, E_WARNING, "Unable to load %s", path->val);
            RETURN_NULL();
        }
    }
    
    // php_printf("initNum:%d, newNum:%d \n", initNum, newNum);
    php_sprintf(separator, "initNum:%d, newNum:%d \n", TRIE_FILTER_G(initNum), TRIE_FILTER_G(newNum));
    php_error(E_WARNING, separator);
    
    RETURN_TRUE;
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
