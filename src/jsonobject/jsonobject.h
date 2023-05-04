#ifndef JPARSE_JSONOBJECT
#define JPARSE_JSONOBJECT

#include <stdbool.h>
#include <stddef.h>

typedef struct json_object json_object;
typedef struct json_array json_array;
typedef struct json_value json_value;
typedef struct json_member json_member;

// Type enumerations:
typedef enum json_value_type {
    JSON_OBJECT = 1,
    JSON_ARRAY,
    JSON_NUMBER,
    JSON_STRING,
    JSON_BOOL,
    JSON_NULL
} json_value_type;


// Function prototypes:
json_object *new_json_object(void);
json_array  *new_json_array(void);
json_member *new_json_member(const char *key, json_value *val);

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

void print_json_value(json_value *val);
void print_json_object(json_object *obj);
void print_json_array(json_array *arr);

#endif /* JPARSE_JSONOBJECT */