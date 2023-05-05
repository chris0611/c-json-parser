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