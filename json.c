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

#include <assert.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>
#include "json.h"

/**************************************************************************************************
	JSON Value  */

json_t json_object()
{
	json_t node = { JSON_NONE };
	json__object_t* object = malloc(sizeof(json__object_t));

	if (object == NULL)
		return node;

	*object = json__object_new(0);

	node.type = JSON_OBJECT;
	node.u.obj = object;
	return node;
}

json_t json_array()
{
	json_t node = { JSON_NONE };
	json__array_t* array = malloc(sizeof(json__array_t));

	if (array == NULL)
		return node;

	*array = json__array_new(0);

	node.type = JSON_ARRAY;
	node.u.arr = array;
	return node;
}

json_t json_string(const char* string)
{
	json_t node = { JSON_NONE };
	json__string_t* data_string = malloc(sizeof(json__string_t));
	int len = (int)strlen(string);

	if (!data_string)
		return node;

	*data_string = json__string_new(len);

	node.type = JSON_STRING;
	node.u.str = data_string;

	json_string_append(node, string, len);
	return node;
}

json_t json_number(double value)
{
	json_t node;
	node.type = JSON_NUMBER;
	node.u.num = value;
	return node;
}

json_t json_bool(int value)
{
	json_t node;
	node.type = value ? JSON_TRUE : JSON_FALSE;
	return node;
}

json_t json_null()
{
	json_t node;
	node.type = JSON_NULL;
	return node;
}

void json_free(json_t value)
{
	int i;
	switch (value.type)
	{
	case JSON_OBJECT:
	{
		json__object_t* object = value.u.obj;

		for (i = 0; i < object->len; i++)
		{
			json_bucket_t* bucket = object->buckets + i;
			json_free(bucket->val);
			free((void*)bucket->key);
		}

		json__object_free(object);
		free(object);
		break;
	}
	case JSON_ARRAY:
	{
		json__array_t* array = value.u.arr;

		for (i = 0; i < array->len; i++)
		{
			json_free(array->data[i]);
		}

		json__array_free(array);
		free(array);
		break;
	}
	case JSON_STRING:
		json__string_free(value.u.str);
		free(value.u.str);
		break;
	}
}

/**************************************************************************************************
	JSON Parser  */

enum json_parse_state
{
	JSON_START = 1 << 0,
	JSON_OBJECT_START = 1 << 1,
	JSON_OBJECT_KEY = 1 << 2,
	JSON_OBJECT_COLON = 1 << 3,
	JSON_OBJECT_VAL = 1 << 4,
	JSON_OBJECT_NEXT = 1 << 5,

	JSON_ARRAY_START = 1 << 6,
	JSON_ARRAY_VAL = 1 << 7,
	JSON_ARRAY_NEXT = 1 << 8,
};

/*	Map any character to JSON token.  */
char json_type[256];

/*	A state mask for every JSON token. Used to tell if a token is valid or out of place.  */
short json_state_mask[JSON_TOKEN_COUNT];

/*	Initialize lookup tables used by the parser.  */
static void json__tables()
{
	static int needs_init = 1;
	int value_states = JSON_OBJECT_VAL | JSON_ARRAY_START | JSON_ARRAY_VAL;
	int i;

	if (!needs_init)
		return;

	/* initialize json type map */

	memset(json_type, JSON_NONE, 256);

	json_type['{'] = JSON_OBJECT;
	json_type['}'] = JSON_SCOPE_END;
	json_type['['] = JSON_ARRAY;
	json_type[']'] = JSON_SCOPE_END;
	json_type['"'] = JSON_STRING;
	json_type[':'] = JSON_COLON;
	json_type[','] = JSON_COMMA;
	json_type['-'] = JSON_NUMBER;

	for (i = 0; i < 10; i++)
		json_type['0' + i] = JSON_NUMBER;

	json_type['t'] = JSON_TRUE;
	json_type['f'] = JSON_FALSE;
	json_type['a'] = JSON_NULL;

	/* initialize json task map */

	json_state_mask[JSON_OBJECT] = JSON_START | value_states;
	json_state_mask[JSON_ARRAY] = JSON_START | value_states;
	json_state_mask[JSON_STRING] = JSON_OBJECT_START | JSON_OBJECT_KEY | value_states;
	json_state_mask[JSON_NUMBER] = value_states;
	json_state_mask[JSON_TRUE] = value_states;
	json_state_mask[JSON_FALSE] = value_states;
	json_state_mask[JSON_NULL] = value_states;
	json_state_mask[JSON_NONE] = 0;
	json_state_mask[JSON_COLON] = JSON_OBJECT_COLON;
	json_state_mask[JSON_COMMA] = JSON_OBJECT_NEXT | JSON_ARRAY_NEXT;
	json_state_mask[JSON_SCOPE_END] = JSON_OBJECT_START | JSON_OBJECT_NEXT
		| JSON_ARRAY_START | JSON_ARRAY_NEXT;

	needs_init = 0;
}

/*	convert string to json_t  */
json_t json_parse(const char* text)
{
	json_t stack[128] = { {JSON_NONE} };
	json_t* sp = stack;

	const char* key = NULL;
	json_t val = { JSON_NONE };

	int state = JSON_START;
	const char* c = text;

	/* initialize lookup tables */

	json__tables();

	/* parse string to json */

	while (*(c = json_skip_whitespace(c)))
	{
		int type = json_type[*c];
		int flags = 0;

		if (!(state & json_state_mask[type]))
			goto end;

		/* parse value */

		switch (type)
		{
		case JSON_OBJECT:
			val = json_object();
			flags = 1;
			c++;
			break;

		case JSON_ARRAY:
			val = json_array();
			flags = 1;
			c++;
			break;

		case JSON_STRING:
			if (state & (JSON_OBJECT_START | JSON_OBJECT_KEY))
			{
				key = json_parse_string_value(&c, 0).data;
				state = JSON_OBJECT_COLON;
				continue;
			}
			else
			{
				val.u.str = malloc(sizeof(json__string_t));

				if (val.u.str == NULL)
				{
					goto end;
				}

				val.type = JSON_STRING;
				*val.u.str = json_parse_string_value(&c, 1);
				break;
			}

		case JSON_NUMBER:
			val = json_number(strtod(c, (char**)&c));
			break;

		case JSON_TRUE:
			if (strncmp(c, "true", 4) == 0)
			{
				val = json_bool(1);
				c += 4;
				break;
			}
			goto end;

		case JSON_FALSE:
			if (strncmp(c, "false", 5) == 0)
			{
				val = json_bool(0);
				c += 5;
				break;
			}
			goto end;

		case JSON_NULL:
			if (strncmp(c, "null", 4) == 0)
			{
				val = json_null();
				c += 4;
				break;
			}
			goto end;

		case JSON_COLON:
			c++;
			state = JSON_OBJECT_VAL;
			continue;

		case JSON_COMMA:
			c++;
			state = sp->type == JSON_OBJECT ? JSON_OBJECT_KEY : JSON_ARRAY_VAL;
			continue;

		case JSON_SCOPE_END:
			if (sp-- == stack)
				goto end;

			c++;
			continue;
		}

		/* add value to parent */

		switch (state)
		{
		case JSON_OBJECT_START:
		case JSON_OBJECT_VAL:
			json__object_set(sp->u.obj, key, val, 0);
			state = JSON_OBJECT_NEXT;
			key = NULL;
			break;

		case JSON_ARRAY_START:
		case JSON_ARRAY_VAL:
			json_array_push(*sp, val);
			state = JSON_ARRAY_NEXT;
			break;

		case JSON_START:
			*sp = val;
			state = val.type == JSON_OBJECT ? JSON_OBJECT_START : JSON_ARRAY_START;
			continue;
		}

		if (flags)
		{
			*(++sp) = val;
			state = val.type == JSON_OBJECT ? JSON_OBJECT_START : JSON_ARRAY_START;
		}
	}

end:
	/*	end of function */

	free((void*)key);
	return *stack;
}

/**************************************************************************************************
	Json Dump  */

static void json_dump_recursive(json_t dst_string, json_t root, int indent)
{
	int i, x;
	switch (root.type)
	{
	case JSON_OBJECT:
		json_string_append(dst_string, "{\n", 2);

		for (i = 0; i < root.u.obj->len; i++)
		{
			int next_indent = indent + 1;
			json_bucket_t* bucket = (json_bucket_t*)root.u.obj->buckets + i;

			for (x = 0; x < next_indent; x++)
				json_string_append(dst_string, "\t", 1);

			json_string_append(dst_string, "\"", 1);
			json_string_append(dst_string, bucket->key, (int)strlen(bucket->key));
			json_string_append(dst_string, "\": ", 3);
			json_dump_recursive(dst_string, bucket->val, next_indent);

			if (i != root.u.obj->len - 1)
				json_string_append(dst_string, ",", 1);

			json_string_append(dst_string, "\n", 1);
		}

		for (x = 0; x < indent; x++)
			json_string_append(dst_string, "\t", 1);

		json_string_append(dst_string, "}", 1);
		break;

	case JSON_ARRAY:
		json_string_append(dst_string, "[\n", 2);

		for (i = 0; i < root.u.arr->len; i++)
		{
			int next_indent = indent + 1;
			json_t* node = (json_t*)root.u.arr->data + i;

			for (x = 0; x < next_indent; x++)
				json_string_append(dst_string, "\t", 1);

			json_dump_recursive(dst_string, *node, next_indent);

			if (i < root.u.arr->len - 1)
				json_string_append(dst_string, ",", 1);

			json_string_append(dst_string, "\n", 1);
		}

		for (x = 0; x < indent; x++)
			json_string_append(dst_string, "\t", 1);

		json_string_append(dst_string, "]", 1);
		break;

	case JSON_STRING:
		json_string_append(dst_string, root.u.str->data, root.u.str->len);
		break;

	case JSON_NUMBER:
		json__string_reserve(dst_string.u.str, dst_string.u.str->len + 0x20);
		dst_string.u.str->len += json_dtoa(root.u.num, 12, dst_string.u.str->data +
			dst_string.u.str->len, dst_string.u.str->cap - dst_string.u.str->len);
		break;

	case JSON_TRUE:
		json_string_append(dst_string, "true", 4);
		break;

	case JSON_FALSE:
		json_string_append(dst_string, "false", 5);
		break;

	case JSON_NULL:
		json_string_append(dst_string, "null", 4);
		break;
	}
}

/*	Dump JSON value to string.  */
json_t json_dump(json_t value)
{
	json_t string = json_string("");

	if (string.type == JSON_NONE)
		return string;

	json_dump_recursive(string, value, 0);
	return string;
}

/**************************************************************************************************
	JSON Object  */

static int json__object_cmp(const char* a, const char* b)
{
	return strcmp(a, b);
}

static int json__object_hash(const char* key)
{
	int multiplier = 31;
	int i, m, hash = 0;

	for (i = 0, m = 1; key[i] != 0; m *= multiplier, i++)
		hash += key[i] * m;

	return hash;
}

json__object_t json__object_new(int len)
{
	json__object_t object;
	int cap = json__next_capacity(len);

	object.buckets = malloc((sizeof(json_bucket_t) + sizeof(int) + sizeof(char)) * (size_t)cap);
	object.sparse = (int*)(object.buckets + cap);
	object.info = (unsigned char*)(object.sparse + cap);
	object.cap = cap;
	object.len = 0;

	memset(object.info, -1, object.cap);
	return object;
}

void json__object_free(json__object_t* object)
{
	free(object->buckets);
}

static void json__object_move(json__object_t* dst, json__object_t* src)
{
	int i;
	for (i = 0; i < src->len; i++)
	{
		json_bucket_t* bucket = src->buckets + i;
		json__object_set(dst, bucket->key, bucket->val, 0);
	}

	json__object_free(src);
	*src = *dst;
}

void json__object_reserve(json__object_t* object, int len)
{
	if (len > (object->cap - (object->cap / 4)))
	{
		json__object_t new_object = json__object_new(len * 2);
		json__object_move(&new_object, object);
	}
}

void json__object_trim(json__object_t* object)
{
	if ((object->cap - (object->cap - (object->cap / 4))) > object->len)
	{
		json__object_t new_object = json__object_new(object->len);
		json__object_move(&new_object, object);
	}
}

json_t* json__object_get_index(json__object_t* object, const char* key, int hash, int* out_index)
{
	int mask = object->cap - 1;
	int idx = hash & mask;
	int distance = 0;

	for (;; idx = (idx + 1) & mask, distance++)
	{
		json_bucket_t* bucket;
		int _distance = object->info[idx];

		if ((_distance == 0xFF) | (distance > _distance))
		{
			*out_index = idx;
			return NULL;
		}

		bucket = object->buckets + object->sparse[idx];

		if (json__object_cmp(key, bucket->key) == 0)
		{
			*out_index = idx;
			return &bucket->val;
		}
	}
}

void json__object_set(json__object_t* object, const char* key, json_t value, int copy_key)
{
	int mask, hash, idx, distance, tmp;
	json_t* val;
	char* name;

	json__object_reserve(object, object->len + 1);

	hash = json__object_hash(key);
	val = json__object_get_index(object, key, hash, &idx);

	if (val != NULL)
	{
		json_free(*val);
		*val = value;
		return;
	}

	if (copy_key)
	{
		tmp = (int)strlen(key) + 1;
		name = memcpy(malloc(tmp), key, tmp);
	}
	else
		name = (char*)key;

	mask = object->cap - 1;
	distance = ((object->cap + idx) - (hash & mask)) & mask;

	for (tmp = -1;; idx = (idx + 1) & mask, distance++)
	{
		int _distance = object->info[idx];
		if (_distance == 0xFF)
		{
			if (tmp == -1)
			{
				/* Add bucket to buckets */
				json_bucket_t* bucket = object->buckets + object->len;
				bucket->key = name;
				bucket->val = value;
				object->sparse[idx] = object->len++;
			}
			else
				/* Replace index */
				object->sparse[idx] = tmp;

			object->info[idx] = distance;
			return;
		}
		else if (distance > _distance)
		{
			int tmp_x = object->sparse[idx];
			if (tmp == -1)
			{
				/* Add bucket to buckets */
				json_bucket_t* bucket = object->buckets + object->len;
				bucket->key = name;
				bucket->val = value;
				object->sparse[idx] = object->len++;
			}
			else
				/* Replace index */
				object->sparse[idx] = tmp;

			tmp = tmp_x;
			object->info[idx] = distance;
			distance = _distance;
		}
	}
}

json_t json_object_get(json_t object, const char* key)
{
	int idx;
	json_t* val, none = { JSON_NONE };

	assert(object.type == JSON_OBJECT);
	val = json__object_get_index(object.u.obj, key, json__object_hash(key), &idx);

	if (val)
		return *val;

	return none;
}

void json_object_set(json_t object, const char* key, json_t value)
{
	assert(object.type == JSON_OBJECT);
	json__object_set(object.u.obj, key, value, 1);
}

json_t json_object_pop(json_t object, const char* key)
{
	int hash, idx, mask, next;
	json_bucket_t* start, * end;
	json__object_t* obj = object.u.obj;
	json_t val = { JSON_NONE };

	assert(object.type == JSON_OBJECT);
	hash = json__object_hash(key);

	if (json__object_get_index(obj, key, hash, &idx) == NULL)
		return val;

	start = obj->buckets + obj->sparse[idx];
	end = obj->buckets + obj->len;

	free((void*)start->key);
	val = start->val;
	memmove(start, start + 1, (size_t)end - (size_t)start);

	obj->len--;
	mask = obj->cap - 1;

	for (next = (idx + 1) & mask;; idx = next, next = (next + 1) & mask)
	{
		int next_distance = obj->info[next];
		if ((next_distance == 0xFF) | (next_distance == 0))
		{
			obj->info[idx] = 0xFF;
			break;
		}

		obj->sparse[idx] = obj->sparse[next];
		obj->info[idx] = next_distance - 1;
	}

	json__object_trim(obj);
	return val;
}

int json_object_erase(json_t object, const char* key)
{
	json_t val = json_object_pop(object, key);
	json_free(val);

	return val.type != JSON_NONE;
}

int json_object_len(json_t object)
{
	assert(object.type == JSON_OBJECT);
	return object.u.obj->len;
}

json_bucket_t* json_object_begin(json_t object)
{
	assert(object.type == JSON_OBJECT);
	return object.u.obj->buckets;
}

json_bucket_t* json_object_end(json_t object)
{
	assert(object.type == JSON_OBJECT);
	return object.u.obj->buckets + object.u.obj->len;
}

json_bucket_t* json_object_at(json_t object, int index)
{
	assert(object.type == JSON_OBJECT && index < object.u.obj->len);
	return object.u.obj->buckets + index;
}

/**************************************************************************************************
	JSON Array  */

json__array_t json__array_new(int len)
{
	json__array_t array = { NULL, 0, 0 };
	json__array_reserve(&array, len);
	return array;
}

void json__array_free(json__array_t* array)
{
	free(array->data);
}

void json__array_reserve(json__array_t* array, int len)
{
	if (array->cap < len)
	{
		json_t* new_data;

		len = json__next_capacity(len);
		new_data = malloc(sizeof(json_t) * (size_t)len);
		array->cap = len;

		memcpy(new_data, array->data, (size_t)array->len * sizeof(json_t));
		json__array_free(array);
		array->data = new_data;
	}
}

void json__array_trim(json__array_t* array)
{
	if ((array->cap / 4) > array->len)
	{
		array->cap = json__next_capacity(array->len * 2);
		array->data = realloc(array->data, (size_t)array->cap * sizeof(json_t));
	}
}

json_t json_array_get(json_t array, int index)
{
	assert(array.type == JSON_ARRAY && index < array.u.arr->len);
	return array.u.arr->data[index];
}

void json_array_set(json_t array, int index, json_t value)
{
	json_t* _node;

	assert(array.type == JSON_ARRAY && index < array.u.arr->len);
	_node = array.u.arr->data + index;

	json_free(*_node);
	*_node = value;
}

void json_array_insert(json_t array, int index, json_t value)
{
	json__array_t* arr = array.u.arr;
	json_t* dst, * src;

	assert(array.type == JSON_ARRAY && index <= array.u.arr->len);

	json__array_reserve(arr, arr->len + 1);
	src = arr->data + index;
	dst = src + 1;
	memmove(dst, src, (size_t)(arr->len - index) * sizeof(json_t));
	*src = value;
	arr->len++;
}

void json_array_push(json_t array, json_t value)
{
	assert(array.type == JSON_ARRAY);
	json_array_insert(array, array.u.arr->len, value);
}

json_t json_array_pop(json_t array, int index)
{
	json__array_t* arr = array.u.arr;
	json_t* src, * dst, val = { JSON_NONE };

	assert(array.type == JSON_ARRAY && index < array.u.arr->len);

	arr->len--;
	dst = arr->data + index;
	src = dst + 1;
	val = *dst;

	memmove(dst, src, (size_t)(arr->len - index) * sizeof(json_t));
	json__array_trim(arr);
	return val;
}

void json_array_erase(json_t array, int index)
{
	json_t val = json_array_pop(array, index);
	json_free(val);
}

int json_array_len(json_t array)
{
	assert(array.type == JSON_ARRAY);
	return array.u.arr->len;
}

json_t* json_array_begin(json_t array)
{
	assert(array.type == JSON_ARRAY);
	return array.u.arr->data;
}

json_t* json_array_end(json_t array)
{
	assert(array.type == JSON_ARRAY);
	return array.u.arr->data + array.u.arr->len;
}

json_t* json_array_at(json_t array, int index)
{
	assert(array.type == JSON_ARRAY && index < array.u.arr->len);
	return array.u.arr->data + index;
}

/**************************************************************************************************
	JSON String  */

json__string_t json__string_new(int len)
{
	json__string_t str = { NULL, 0, 0 };
	json__string_reserve(&str, len);
	return str;
}

void json__string_free(json__string_t* str)
{
	free(str->data);
}

void json__string_reserve(json__string_t* str, int len)
{
	if (str->cap <= len)
	{
		char* new_data;

		str->cap = json__next_capacity(len + 1);
		new_data = (char*)malloc(str->cap);
		memcpy(new_data, str->data, str->len);
		new_data[str->len] = 0;
		json__string_free(str);
		str->data = new_data;
	}
}

void json__string_trim(json__string_t* str)
{
	if ((str->cap / 4) > str->len)
	{
		str->cap = json__next_capacity(str->len * 2);
		str->data = realloc(str->data, (size_t)str->cap);
	}
}

void json_string_insert(json_t str, const char* seq, int len, int idx)
{
	json__string_t* string = str.u.str;

	assert(str.type == JSON_STRING);

	json__string_reserve(string, string->len + len);
	memmove(string->data + idx + len, string->data + idx, string->len - idx);
	memcpy(string->data + idx, seq, len);
	string->len += len;
	string->data[string->len] = 0;
}

void json_string_append(json_t str, const char* seq, int len)
{
	assert(str.type == JSON_STRING);
	json_string_insert(str, seq, len, str.u.str->len);
}

void json_string_erase(json_t str, int idx, int len)
{
	json__string_t* string = str.u.str;

	assert(str.type == JSON_STRING);

	if ((idx + len) > string->len)
		return;

	memmove(string->data + idx, string->data + idx + len, string->len - idx - len);
	string->len -= len;
	string->data[string->len] = 0;
	json__string_trim(string);
}

int json_string_len(json_t str)
{
	assert(str.type == JSON_STRING);
	return str.u.str->len;
}

char* json_string_begin(json_t str)
{
	assert(str.type == JSON_STRING);
	return str.u.str->data;
}

const char* json_string_end(json_t str)
{
	assert(str.type == JSON_STRING);
	return str.u.str->data + str.u.str->len;
}

char* json_string_at(json_t str, int idx)
{
	assert(str.type == JSON_ARRAY && idx < str.u.str->len);
	return str.u.str->data + idx;
}

/**************************************************************************************************
	Helper functions  */

int json__next_capacity(int len)
{
	unsigned int value = (unsigned int)(len - 1);
	int result = 0;

	if ((value & 0xFFFF0000) == 0)
	{
		result += 16;
		value <<= 16;
	}
	if ((value & 0xFF000000) == 0)
	{
		result += 8;
		value <<= 8;
	}
	if ((value & 0xF0000000) == 0)
	{
		result += 4;
		value <<= 4;
	}
	if ((value & 0xC0000000) == 0)
	{
		result += 2;
		value <<= 2;
	}

	result += ((~value) & 0x80000000) != 0;
	result = 31 - result;

	return (len < 4) ? 4 : 1 << (result + 1);
}

int json_dtoa(double value, int precision, char* buffer, int size)
{
	int exponent;
	double divisor;
	char* start = buffer;

	if (isnan(value))
	{
		memcpy(buffer, "nan", 4);
		return 3;
	}

	else if (isinf(value))
	{
		memcpy(buffer, "inf", 4);
		return 3;
	}

	if (value < 0.0)
	{
		value = -value;
		*buffer++ = '-';
	}

	exponent = (int)log10(value);

	if (exponent < 0)
		exponent = 0;

	divisor = pow(10.0, (double)exponent);

	while (precision)
	{
		double result = floor(value / divisor);

		double digit = fmod(result, 10);

		if (exponent == -1)
			*buffer++ = '.';

		*buffer++ = '0' + (int)digit;

		if (exponent <= 0 && value - result * divisor <= 0.0)
		{
			exponent--;
			break;
		}

		divisor /= 10.0;
		exponent--;
		precision--;
	}

	if (exponent >= 0)
	{
		int div = (int)pow(10, floor(log10(++exponent)));

		*buffer++ = 'e';
		*buffer++ = '+';

		for (; div > 0; div /= 10)
			*buffer++ = '0' + (exponent / div) % 10;
	}

	*buffer = 0;
	return (int)(buffer - start);
}

const char* json_skip_whitespace(const char* c)
{
	while (*c && *c <= 0x20) c++;
	return c;
}

json__string_t json_parse_string_value(const char** p, int pad)
{
	json__string_t str = { NULL, 0, 0 };
	const char* head = *p + 1;
	const char* c = head;

	while ((c = strchr(c, '"')) && *(c - 1) == '\\') c++;

	if (c)
	{
		str.len = (int)(c - head);
		str.cap = pad ? json__next_capacity(str.len + 1) : str.len + 1;
		str.data = malloc(str.cap);

		if (str.data == NULL)
		{
			str.len = 0;
			str.cap = 0;
			return str;
		}

		memcpy(str.data, head, str.len);
		str.data[str.len] = 0;
		*p = c + 1;
		return str;
	}

	*p = head;
	return str;
}