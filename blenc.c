/*
  +----------------------------------------------------------------------+
  | PHP Version 8                                                        |
  +----------------------------------------------------------------------+
  | Copyright (c) 1997-2013 The PHP Group                                |
  +----------------------------------------------------------------------+
  | This source file is subject to version 3.0 of the PHP license,       |
  | that is bundled with this package in the file LICENSE, and is        |
  | available through the world-wide-web at the following url:           |
  | http://www.php.net/license/3_0.txt.                                  |
  | If you did not receive a copy of the PHP license and are unable to   |
  | obtain it through the world-wide-web, please send a note to          |
  | license@php.net so we can mail you a copy immediately.               |
  +----------------------------------------------------------------------+
  | Author: John Coggeshall <john@php.net>                               |
  |         Giuseppe Chiesa <gchiesa@php.net>			 				 |
  +----------------------------------------------------------------------+
*/

/* $Id$ */

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include "php.h"
#include "php_ini.h"
#include "ext/standard/info.h"
#include "ext/standard/md5.h"
#include "ext/standard/base64.h"
#ifdef PHP_WIN32
# include "win32/time.h"
#endif
#include "php_blenc.h"
#include "blenc_protect.h"
#include "bf_algo.h"

//blenc_encrypt ( string $plaintext , string $encodedfile , string $encryption_key = ? ) : string

// the first argument is the name of this arginfo block
// the second argument is always zero (reserved for future use)
// the third argument is if it returns a reference
// the fourth argument is how many arguments are required
ZEND_BEGIN_ARG_INFO_EX(arginfo_blenc, 0, 0, 2)
// the first argument to this is if this is a reference
// the second argument to this is the name
ZEND_ARG_INFO(0, plaintext)
ZEND_ARG_INFO(0, encodedfile)
ZEND_ARG_INFO(0, encryption_key)
ZEND_END_ARG_INFO()





ZEND_DECLARE_MODULE_GLOBALS(blenc)

typedef struct _blenc_header {
	b_byte ident[8];
	b_byte version[16];
	b_byte md5[32];
	b_byte reserved[16];
} blenc_header;

/* True global - no need for thread safety here */
HashTable *php_bl_keys;

/* {{{ blenc_functions[] */
zend_function_entry blenc_functions[] = {
	PHP_FE(blenc_encrypt,	arginfo_blenc)
	{
		NULL, NULL, NULL
	}
};
/* }}} */

/* {{{ blenc_module_entry
 */
zend_module_entry blenc_module_entry = {
#if ZEND_MODULE_API_NO >= 20010901
	STANDARD_MODULE_HEADER,
#endif
	"blenc",
	blenc_functions,
	PHP_MINIT(blenc),
	PHP_MSHUTDOWN(blenc),
	PHP_RINIT(blenc),
	NULL,
	PHP_MINFO(blenc),
#if ZEND_MODULE_API_NO >= 20010901
	PHP_BLENC_VERSION, /* Replace with version number for your extension */
#endif
	STANDARD_MODULE_PROPERTIES
};
/* }}} */

#ifdef COMPILE_DL_BLENC
ZEND_GET_MODULE(blenc)
#endif

/* {{{ PHP_INI */
PHP_INI_BEGIN()
STD_PHP_INI_ENTRY("blenc.key_file", "", PHP_INI_ALL, OnUpdateString, key_file, zend_blenc_globals, blenc_globals)
PHP_INI_END()
/* }}} */

/* {{{ php_blenc_init_globals */
static void php_blenc_init_globals(zend_blenc_globals *blenc_globals)
{
	blenc_globals->key_file = NULL;
	blenc_globals->keys_loaded = FALSE;
	blenc_globals->decoded = NULL;
	blenc_globals->decoded_len = 0;
	blenc_globals->index = 0;

}
/* }}} */

/* {{{ PHP_MINIT_FUNCTION
 */
PHP_MINIT_FUNCTION(blenc)
{
	time_t now;
	struct tm *today;
	char today_char[16];
	char expire_char[] = BLENC_PROTECT_EXPIRE;
	char buff[10];
	int k = 0, j = 0, i = 0;

	ZEND_INIT_MODULE_GLOBALS(blenc, php_blenc_init_globals, NULL);
	REGISTER_INI_ENTRIES();

	php_bl_keys = pemalloc(sizeof(HashTable), TRUE);
	zend_hash_init(php_bl_keys, 0, NULL, (dtor_func_t)_php_blenc_pefree_wrapper, TRUE);

	zend_compile_file_old = zend_compile_file;
	zend_compile_file = blenc_compile;

	REGISTER_STRING_CONSTANT("BLENC_EXT_VERSION", PHP_BLENC_VERSION, CONST_CS | CONST_PERSISTENT);

	/*
	 * check expire extension
	 */
	memset(today_char, '\0', sizeof(today_char));
	now = time(NULL);
	today = localtime(&now);
	strftime(today_char, 16, "%Y%m%d", today);

#ifndef BLENC_PROTECT_COMP3
	BL_G(expire_date) = pemalloc(sizeof(expire_char), TRUE);
	strcpy(BL_G(expire_date), expire_char);

	strncpy(buff, &expire_char[6], 4);
	strncpy(&buff[4], &expire_char[3], 2);
	strncpy(&buff[6], &expire_char[0], 2);
	buff[8] = '\0';
#else
	j = 0;
	for(k=0; k<4; k++) {

		if(expire_char[k] == 0) {

			buff[j++] = '0';
			buff[j++] = '0';

		} else {

			i = expire_char[k];
			if(i < 0)
				i += 256;

			buff[j++] = i / 16 + '0';
			buff[j++] = i % 16 + '0';

		}

	} // for
	buff[j] = '\0';

	BL_G(expire_date) = pemalloc(11, TRUE);
	strncpy(&BL_G(expire_date)[0], &buff[6], 2);
	BL_G(expire_date)[2] = '-';
	strncpy(&BL_G(expire_date)[3], &buff[4], 2);
	BL_G(expire_date)[5] = '-';
	strncpy(&BL_G(expire_date)[6], &buff[0], 4);
	BL_G(expire_date)[10] = '\0';
#endif

	if(atol(today_char) > atol(buff))
		BL_G(expired) = TRUE;

	return SUCCESS;
}
/* }}} */

/* {{{ PHP_MSHUTDOWN_FUNCTION
 */
PHP_MSHUTDOWN_FUNCTION(blenc)
{
	UNREGISTER_INI_ENTRIES();

	zend_hash_destroy(php_bl_keys);
	pefree(php_bl_keys, TRUE);
	// FREE_HASHTABLE(php_bl_keys);
	zend_compile_file = zend_compile_file_old;

	return SUCCESS;
}
/* }}} */

/* Remove if there's nothing to do at request start */
/* {{{ PHP_RINIT_FUNCTION
 */
PHP_RINIT_FUNCTION(blenc)
{
	if(!BL_G(keys_loaded)) {
		if(php_blenc_load_keyhash() == FAILURE) {
			zend_error(E_WARNING, "BLENC: Could not load some or all of the Keys");
			return FAILURE;
		}
		BL_G(keys_loaded) = TRUE;
	}

	return SUCCESS;
}
/* }}} */

/* {{{ PHP_MINFO_FUNCTION
 */
PHP_MINFO_FUNCTION(blenc)
{
	php_info_print_table_start();

	if(BL_G(expired))
		php_info_print_table_row(2, "Blenc support", "Expired");
	else
		php_info_print_table_row(2, "Blenc support", "Enabled");

	php_info_print_table_row(2, "Blenc version", PHP_BLENC_VERSION);
	php_info_print_table_row(2, "Blenc expire date", BL_G(expire_date));

	php_info_print_table_end();

	DISPLAY_INI_ENTRIES();

}
/* }}} */

PHP_FUNCTION(blenc_encrypt)
{

	unsigned char *key = NULL, *retval = NULL;
	char *data = NULL, *output_file = NULL;
	int output_len = 0, key_len = 0, data_len = 0, output_file_len = 0;
	php_stream *stream;
	//zend_bool dup_key = FALSE;
	char main_key[] = BLENC_PROTECT_MAIN_KEY;
	char main_hash[33];
	b_byte *bfdata = NULL;

	int bfdata_len = 0;
	zend_string *b64data = NULL;
	char *rtnstr = NULL;
	int b64data_len = 0;
	blenc_header header = {BLENC_IDENT, PHP_BLENC_VERSION};

	memset(main_hash, '\0', sizeof(main_hash));

	php_blenc_make_md5(main_hash, main_key, strlen(main_key));

	if(zend_parse_parameters(ZEND_NUM_ARGS(), "ss|s", &data, &data_len,
	                         &output_file, &output_file_len,
	                         &key, &key_len) == FAILURE) {
		RETURN_FALSE;
	}

	if(key == NULL) {
		key = php_blenc_gen_key();
	} //else {
	//	dup_key = TRUE;
	//}

	if(strlen(data) > 0 && data_len == 0) {
		data_len = strlen(data);
	}
	php_blenc_make_md5((char *)&header.md5, data, data_len);
	retval = php_blenc_encode(data, key, data_len, &output_len);
	bfdata = php_blenc_encode(key, (unsigned char *)main_hash, strlen((char *)key), &bfdata_len);
	b64data = php_base64_encode(bfdata, bfdata_len);

	if((stream = php_stream_open_wrapper(output_file, "wb", REPORT_ERRORS, NULL))) {

		_php_stream_write(stream, (void *)&header, (int)sizeof(blenc_header));
		_php_stream_write(stream, (char *)retval, output_len);
		php_stream_close(stream);

		/* RETVAL_STRING(key, dup_key); */
		rtnstr = strdup(ZSTR_VAL(b64data));
		zend_string_release(b64data);
		RETVAL_STRINGL((char *)rtnstr, ZSTR_LEN(b64data));
	}

	efree(retval);
}

static void php_blenc_make_md5(char *result, void *data, unsigned int data_len)
{
	PHP_MD5_CTX   context;
	unsigned char digest[16];

	PHP_MD5Init(&context);
	PHP_MD5Update(&context, data, data_len);
	PHP_MD5Final(digest, &context);

	make_digest(result, digest);

}

static char *php_blenc_file_to_mem(char *filename)
{
	php_stream *stream;
	int len;
	zend_string *data = NULL;
	char *rtnstr = NULL;
	zval *stringvalue = NULL;

	if (!(stream = php_stream_open_wrapper(filename, "rb", REPORT_ERRORS, NULL))) {
		return NULL;
	}

	data = php_stream_copy_to_mem(stream, PHP_STREAM_COPY_ALL, 0);
	if (data == NULL || ZSTR_LEN(data) == 0) {
		data = zend_string_init("", 0, 0);
	}

	php_stream_close(stream);

	if(data == NULL) {
		return NULL;
	}

	rtnstr = estrdup(ZSTR_VAL(data));
	zend_string_release(data);
	return rtnstr;
}

static int php_blenc_load_keyhash()
{
	char *strtok_buf = NULL;
	char *key = NULL;
	char *keys = NULL;
	char main_key[] = BLENC_PROTECT_MAIN_KEY;
	unsigned char main_hash[33];
	b_byte *buff = NULL;
	int buff_len = 0;
	unsigned char *bfdata = NULL;
	zend_string *zstring_bfdata = NULL;
	zval *rtncode = NULL;
	int bfdata_len = 0;

    // Graceful fail if no keyfile present
    if(BL_G(key_file) == ""){
        return SUCCESS;
    }

	keys = php_blenc_file_to_mem(BL_G(key_file));

	memset(main_hash, '\0', sizeof(main_hash));
	php_blenc_make_md5((char *)main_hash, main_key, strlen(main_key));

	if(keys) {
		char *t = keys;

		while((key = php_strtok_r(t, "\n", &strtok_buf))) {
			char *temp;
			t = NULL;

			if(!key) {
				continue;
			}

			zstring_bfdata = php_base64_decode((unsigned char *)key, strlen(key));
			bfdata = estrdup(ZSTR_VAL(zstring_bfdata));
			bfdata_len = zstring_bfdata->len;
			zend_string_release(zstring_bfdata);
			buff = php_blenc_decode(bfdata, main_hash, bfdata_len, &buff_len);

			temp = pestrdup((char *)buff, TRUE);
			efree(bfdata);
			bfdata = NULL;
			rtncode = zend_hash_next_index_insert(php_bl_keys, (zval *)&temp);
			if(Z_TYPE_P(rtncode) == IS_FALSE) {
				zend_error(E_WARNING, "Could not add a key to the keyhash!");
			}
			temp = NULL;

		}

		efree(keys);

	}

	return SUCCESS;

}


b_byte *php_blenc_encode(void *script, unsigned char *key, int in_len, int *out_len)
{
	BLOWFISH_CTX *ctx = NULL;
	unsigned long hi = 0, low = 0;
	int i, pad_size = 0;
	b_byte *input = NULL;

	ctx = emalloc(sizeof(BLOWFISH_CTX));

	Blowfish_Init (ctx, (unsigned char *)key, strlen((char *)key));


	if((pad_size = in_len % 8)) {
		pad_size = 8 - pad_size;

		input = (b_byte*) estrdup(script);
		input = erealloc(input, in_len + pad_size);

		memset(&input[in_len], '\0', pad_size);

	} else {

		input = (b_byte*) estrdup(script);
		pad_size = 0;

	}

	hi = 0x0L;
	low = 0x0L;

	for(i = 0; i < (in_len + pad_size); i+=8) {

		hi |= (unsigned int)((char *)input)[i] & 0xFF;
		hi = hi << 8;
		hi |= (unsigned int)((char *)input)[i+1] & 0xFF;
		hi = hi << 8;
		hi |= (unsigned int)((char *)input)[i+2] & 0xFF;
		hi = hi << 8;
		hi |= (unsigned int)((char *)input)[i+3] & 0xFF;

		low |= (unsigned int)((char *)input)[i+4] & 0xFF;
		low = low << 8;
		low |= (unsigned int)((char *)input)[i+5] & 0xFF;
		low = low << 8;
		low |= (unsigned int)((char *)input)[i+6] & 0xFF;
		low = low << 8;
		low |= (unsigned int)((char *)input)[i+7] & 0xFF;

		Blowfish_Encrypt(ctx, &hi, &low);

		input[i] = hi >> 24;
		input[i+1] = hi >> 16;
		input[i+2] = hi >> 8;
		input[i+3] = hi;
		input[i+4] = low >> 24;
		input[i+5] = low >> 16;
		input[i+6] = low >> 8;
		input[i+7] = low;

		hi = 0x0L;
		low = 0x0L;
	}

	*out_len = in_len + pad_size;

	efree(ctx);

	return input;
}

b_byte *php_blenc_decode(void *input, unsigned char *key, int in_len, int *out_len)
{
	BLOWFISH_CTX ctx;
	unsigned long hi, low;
	unsigned long retval_size = 0;
	int i;
	b_byte *retval;

	Blowfish_Init (&ctx, (unsigned char*)key, strlen((char *)key));

	if(in_len % 8) {

		zend_error(E_WARNING, "Attempted to decode non-blenc encrytped file.");
		return (b_byte *)estrdup("");

	} else {

		retval = emalloc(in_len + 1);

	}

	retval_size = sizeof(retval);
	memset(retval, '\0', retval_size);

	hi = 0x0L;
	low = 0x0L;

	for(i = 0; i < in_len; i+=8) {

		hi |= (unsigned int)((char *)input)[i] & 0xFF;
		hi = hi << 8;
		hi |= (unsigned int)((char *)input)[i+1] & 0xFF;
		hi = hi << 8;
		hi |= (unsigned int)((char *)input)[i+2] & 0xFF;
		hi = hi << 8;
		hi |= (unsigned int)((char *)input)[i+3] & 0xFF;

		low |= (unsigned int)((char *)input)[i+4] & 0xFF;
		low = low << 8;
		low |= (unsigned int)((char *)input)[i+5] & 0xFF;
		low = low << 8;
		low |= (unsigned int)((char *)input)[i+6] & 0xFF;
		low = low << 8;
		low |= (unsigned int)((char *)input)[i+7] & 0xFF;

		Blowfish_Decrypt(&ctx, &hi, &low);

		retval[i] = hi >> 24;
		retval[i+1] = hi >> 16;
		retval[i+2] = hi >> 8;
		retval[i+3] = hi;
		retval[i+4] = low >> 24;
		retval[i+5] = low >> 16;
		retval[i+6] = low >> 8;
		retval[i+7] = low;

		hi = 0x0L;
		low = 0x0L;

	}

	retval[in_len] = '\0';
	*out_len = strlen((char *)retval);

	return retval;
}

static unsigned char *php_blenc_gen_key()
{

	int sec = 0 , usec = 0;
	struct timeval tv;
	char *retval = NULL, *tmp = NULL;
	PHP_MD5_CTX   context;
	unsigned char digest[16];

	gettimeofday((struct timeval *) &tv, (struct timezone *) NULL);
	sec = (int) tv.tv_sec;
	usec = (int) (tv.tv_usec % 0x100000);

	spprintf(&tmp, 0, "%08x%05x", sec, usec);

	retval = emalloc(33);

	PHP_MD5Init(&context);
	PHP_MD5Update(&context, tmp, strlen(tmp));
	PHP_MD5Final(digest, &context);
	make_digest(retval, digest);
	efree(tmp);

	return (unsigned char *)retval;
}

zend_op_array *blenc_compile(zend_file_handle *file_handle, int type)
{

	int i = 0;
	size_t bytes;
	php_stream *stream;
	char *script = NULL;
	b_byte *decoded = NULL;
	int decoded_len = 0;
	unsigned int index = 0;
	unsigned int script_len = 0;
	zend_op_array *retval = NULL;
	blenc_header *header;
	zend_bool validated = FALSE;

	/*
	 * using php_stream instead zend internals
	 */
	if( (stream = php_stream_open_wrapper(file_handle->filename, "r", REPORT_ERRORS, NULL)) == NULL) {
		zend_error(E_NOTICE, "blenc_compile: unable to open stream, compiling with default compiler. Filename %s\n", file_handle->filename);
		return retval = zend_compile_file_old(file_handle, type);
	}

	script = emalloc(BLENC_BUFSIZE);
	for(i = 2; (bytes = php_stream_read(stream, &script[index], BLENC_BUFSIZE)) > 0; i++) {

		script_len += bytes;
		if(bytes == BLENC_BUFSIZE) {

			script = erealloc(script, BLENC_BUFSIZE * i);
			index += bytes;

		}

	}

	script_len += bytes;

	php_stream_close(stream);

	if(!script_len) {

		zend_error(E_NOTICE, "blenc_compile: unable to read stream, compiling with default compiler.");
		return retval = zend_compile_file_old(file_handle, type);

	}

	/*
	 * check if it's a blenc script
	 */
	header = (blenc_header *)script;

	if(!strncmp(script, BLENC_IDENT, strlen(BLENC_IDENT))) {

		char *md5;
		char *encoded = &script[sizeof(blenc_header)];
		unsigned char *key = NULL;

		if(BL_G(expired)) {

			zend_error(E_ERROR, "blenc_compile: Module php_blenc was expired. Please buy a new license key or disable the module.");
			return NULL;

		}

		ZEND_HASH_FOREACH_PTR(php_bl_keys, key)

			decoded = php_blenc_decode(encoded, key, script_len - sizeof(blenc_header), &decoded_len);
			md5 = emalloc(33);
			php_blenc_make_md5(md5, decoded, decoded_len);

			if(!strncmp(md5, (char *)header->md5, 32)) {

				validated = TRUE;
				efree(md5);
				break;

			}

			zend_error(E_WARNING, "blenc_compile: Validation of script '%s' failed. MD5_FILE: %s MD5_CALC: %s\n",
			           file_handle->filename, header->md5, md5);

			efree(md5);
			md5 = NULL;

			efree(decoded);
			decoded_len = 0;

		ZEND_HASH_FOREACH_END();

        if(!validated) {

			zend_error(E_ERROR, "blenc_compile: Validation of script '%s' failed, cannot execute.", file_handle->filename);
			return NULL;

		}

	}

	if(validated && decoded != NULL)
	{
		zend_string* x = zend_string_init((char*)decoded, decoded_len, 0);
		retval = zend_compile_string(x, (char *)file_handle->filename);
	} else
	{
		retval = zend_compile_file_old(file_handle, type);

	}
	return retval;
}

void _php_blenc_pefree_wrapper(void **data)
{
	pefree(*data, TRUE);

}

/*
 * Local variables:
 * tab-width: 4
 * c-basic-offset: 4
 * End:
 * vim600: noet sw=4 ts=4 fdm=marker
 * vim<600: noet sw=4 ts=4
 */
