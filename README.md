# json-c89

Lightweight C89 JSON library with dynamic data structures

## Usage

Just copy `json.h` and `json.c` to your project files.

## Features

- Fast JSON parser
- Dynamic data structures
  - Object (Robin Hood Hashmap)
  - Array
  - String

## Notes

Values inside objects or arrays will be freed automatically. ALL other values must be freed using `json_free()`.

## Examples

Object

```C
json_t object = json_object();

json_object_set(object, "name", json_string("Luke"));
json_object_set(object, "age", json_number(29));

json_free(object);
```

Array

```C
json_t array = json_array();

json_array_append(array, json_string("Luke"));
json_array_append(array, json_number(29));

json_free(array);
```

Object iteration

```C
json_bucket_t *i;
for (i = json_object_begin(object); i != json_object_end(object); i++)
{
    const char* key = i->key;
    json_t val = i->val;
}
```

Array iteration

```C
json_t *i;
for (i = json_array_begin(array); i != json_array_end(array); i++)
{
    json_t val = *i;
}
```

Parse

```C
json_t value = json_parse(string);
```

Dump

```C
json_t string = json_dump(val);
```

## License

[MIT No Attribution](LICENSE)