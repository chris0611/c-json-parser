/*
MIT License

Copyright (c) 2023 chris0611

Permission is hereby granted, free of charge, to any person obtaining a copy
of this software and associated documentation files (the "Software"), to deal
in the Software without restriction, including without limitation the rights
to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
copies of the Software, and to permit persons to whom the Software is
furnished to do so, subject to the following conditions:

The above copyright notice and this permission notice shall be included in all
copies or substantial portions of the Software.

THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
SOFTWARE.
*/


#ifndef JPARSE_JSONOBJECT
#define JPARSE_JSONOBJECT

#include <stdbool.h>
#include <stddef.h>

typedef struct json_object json_object;
typedef struct json_array json_array;
typedef struct json_value json_value;
typedef struct json_member json_member;

// Function prototypes:
json_object *new_json_object(void);
json_array  *new_json_array(void);
json_member *new_json_member(const char *key, json_value *val);

/* Assuming `val` contains the member specified in `path`, its value is returned.
   Otherwise NULL. */
const json_value *get_value_from_path(const char *path, json_value *val);

const char *get_key_from_member(const json_member *mbr);

size_t size_json_object(const json_object *obj);
size_t size_json_array(const json_array *arr);

json_value *json_arr(json_array *arr);
json_value *json_obj(json_object *obj);
json_value *json_null(void);
json_value *json_str(const char *str);
json_value *json_bool(bool boolean);
json_value *json_num(double num);

void dispose_json_value(json_value *val);
void dispose_json_member(json_member *mbr);
void dispose_json_object(json_object *obj);
void dispose_json_array(json_array *arr);
bool add_member(json_object *obj, const json_member *mem);
bool add_value(json_array *arr, const json_value *val);

char *stringify_json_value(const json_value *val);

void print_json_value(const json_value *val);
void print_json_object(const json_object *obj);
void print_json_array(const json_array *arr);

#endif /* JPARSE_JSONOBJECT */