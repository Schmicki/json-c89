/**************************************************************************************************

	MIT No Attribution

	Copyright 2023 Nick Wettstein

	Permission is hereby granted, free of charge, to any person obtaining a
	copy of this software and associated documentation files (the "Software"),
	to deal in the Software without restriction, including without limitation
	the rights to use, copy, modify, merge, publish, distribute, sublicense,
	and/or sell copies of the Software, and to permit persons to whom the
	Software is furnished to do so.

	THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
	IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
	FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
	AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
	LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING
	FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER
	DEALINGS IN THE SOFTWARE.

**************************************************************************************************/

#pragma once

/**************************************************************************************************

	Definitions

**************************************************************************************************/

enum json_tokens
{
	/*	Types  */

	JSON_OBJECT,
	JSON_ARRAY,
	JSON_STRING,
	JSON_NUMBER,
	JSON_TRUE,
	JSON_FALSE,
	JSON_NULL,
	JSON_NONE,

	/*	Others  */

	JSON_COLON,
	JSON_COMMA,
	JSON_SCOPE_END,
	JSON_TOKEN_COUNT,
};

/*************************************************************************************************/

/* JSON Value, 16 bytes */
typedef struct json_t
{
	int type;
	union {
		struct json__object_t* obj;
		struct json__array_t* arr;
		struct json__string_t* str;
		double num;
	} u;

} json_t;

/*************************************************************************************************/

/* 24 bytes */
typedef struct json_bucket_t
{
	const char* key;
	json_t val;

} json_bucket_t;

/*************************************************************************************************/

/*	20 - 32 bytes  */
typedef struct json__object_t
{
	json_bucket_t* buckets;
	int* sparse;
	unsigned char* info;
	int len;
	int cap;

} json__object_t;

/*************************************************************************************************/

/*	12 - 16 bytes  */
typedef struct json__array_t
{
	json_t* data;
	int len;
	int cap;

} json__array_t;

/*************************************************************************************************/

/*	12 - 16 bytes  */
typedef struct json__string_t
{
	char* data;
	int len;
	int cap;

} json__string_t;


/**************************************************************************************************

	Function declarations

***************************************************************************************************
	JSON Value  */

json_t json_object();
json_t json_array();
json_t json_string(const char* string);
json_t json_number(double value);
json_t json_bool(int value);
json_t json_null();
void json_free(json_t value);
json_t json_parse(const char* data);
json_t json_dump(json_t value);

/**************************************************************************************************
	JSON Object  */

json__object_t json__object_new(int len);
void json__object_free(json__object_t* object);
void json__object_reserve(json__object_t* object, int len);
void json__object_trim(json__object_t* object);
json_t* json__object_get_index(json__object_t* object, const char* key, int hash, int* out_index);
void json__object_set(json__object_t* object, const char* key, json_t value, int copy_key);

json_t json_object_get(json_t object, const char* key);
void json_object_set(json_t object, const char* key, json_t value);
json_t json_object_pop(json_t object, const char* key);
int json_object_erase(json_t object, const char* key);
int json_object_len(json_t object);

/*	IMPORTANT:
	- NEVER change the key!
	- Call 'json_free()' on bucket->val before you replace it!  */
json_bucket_t* json_object_begin(json_t object);

/*	IMPORTANT:
	- NEVER change the key!
	- Call 'json_free()' on bucket->val before you replace it!  */
json_bucket_t* json_object_end(json_t object);

/*	IMPORTANT:
	- NEVER change the key!
	- Call 'json_free()' on bucket->val before you replace it!  */
json_bucket_t* json_object_at(json_t object, int index);

/**************************************************************************************************
	JSON Array  */

json__array_t json__array_new(int len);
void json__array_free(json__array_t* array);
void json__array_reserve(json__array_t* array, int len);
void json__array_trim(json__array_t* array);

json_t json_array_get(json_t array, int index);
void json_array_set(json_t array, int index, json_t value);
void json_array_insert(json_t array, int index, json_t value);
void json_array_push(json_t array, json_t value);
json_t json_array_pop(json_t array, int index);
void json_array_erase(json_t array, int index);
int json_array_len(json_t array);

/*	IMPORTANT: Call 'json_free()' on old value before you replace it!  */
json_t* json_array_begin(json_t array);

/*	IMPORTANT: Call 'json_free()' on old value before you replace it!  */
json_t* json_array_end(json_t array);

/*	IMPORTANT: Call 'json_free()' on old value before you replace it!  */
json_t* json_array_at(json_t array, int index);

/**************************************************************************************************
	JSON String  */

json__string_t json__string_new(int len);
void json__string_free(json__string_t* str);
void json__string_reserve(json__string_t* str, int len);

/*	Trim string to lowest fitting capacity  */
void json__string_trim(json__string_t* str);

void json_string_insert(json_t str, const char* seq, int len, int idx);
void json_string_append(json_t str, const char* seq, int len);
void json_string_erase(json_t str, int idx, int len);
int json_string_len(json_t str);

char* json_string_begin(json_t str);
const char* json_string_end(json_t str);
char* json_string_at(json_t str, int idx);

/**************************************************************************************************
	Helper functions  */

int json__next_capacity(int len);
int json_dtoa(double value, int precision, char* buffer, int size);
const char* json_skip_whitespace(const char* c);

/*	If pad is enabled more memory than needed will be allocated. This reduces the number of reallo-
	cations if the string grows. p will be updated to point to the next character after the string
	ended.  */
json__string_t json_parse_string_value(const char** p, int pad);
