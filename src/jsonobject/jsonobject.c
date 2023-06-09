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


#include "jsonobject.h"

#define __STDC_WANT_IEC_60559_BFP_EXT__ // strfromd()

#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <assert.h>
#include <string.h>

#define LOAD_FACTOR (60)        // Grow Hashmap if 60% filled
#define INIT_CAPACITY (64)      // Initial capacity of array
#define DOWNSIZE_LIMIT (25)     // Shrink if less than 25% filled


#define SET_INDENT(ptr, level)                      \
    do {                                            \
        char *tmp = (ptr);                          \
        tmp = alloca((level + 1) * sizeof(char));   \
        memset(tmp, ' ', level); tmp[level] = '\0'; \
        ptr = tmp;                                  \
    } while (0)

#define KEY_COLOR(str)      "\033[38;5;132m" str "\033[m"
#define LITERAL_COLOR(str)  "\033[38;5;108m" str "\033[m"
#define STRING_COLOR(str)   "\033[38;5;145m" str "\033[m"

// Type enumerations:
typedef enum json_value_type {
    JSON_OBJECT = 1,
    JSON_ARRAY,
    JSON_NUMBER,
    JSON_STRING,
    JSON_BOOL,
    JSON_NULL
} json_value_type;

// Structure definitions:
struct json_value {
    union {
        struct json_object *obj;
        struct json_array *arr;
        double number;
        char *string;
        bool boolean;
    };
    json_value_type type;
};

struct json_member {
    const char *key;
    struct json_value *val;
};

struct json_array {
    size_t size;
    size_t capacity;
    struct json_value **values;
};

struct json_object {
    size_t size;
    size_t capacity;
    struct json_member **members;
};


// Static function prototypes
static void print_array_indent(const json_array *arr, size_t level, bool ismbr);
static void print_object_indent(const json_object *obj, size_t level, bool ismbr);
static void print_value_indent(const json_value *val, size_t level, bool ismbr);
static void print_member_indent(const json_member *mbr, size_t level);


static uint64_t hash_key(const char *key) {
    uint64_t hash = 5381;
    int c;

    while ((c = *key++))
        hash = ((hash << 5) + hash) + c; /* hash * 33 + c */
    return hash;
}


static uint64_t quad_probe(const uint64_t idx) {
    // c_1 = c_2 = 1/2
    //return idx/2  + (idx * idx)/2;

    (void)idx;
    // linear probing for now
    return 1;
}


static bool handle_collision(const json_member ** mbrs, size_t len,
                             size_t start_idx, const json_member *mbr) {
    // Do something smarter than linear probing?
    size_t idx = start_idx;

    // Should _always_ have an open spot
    while (true) {
        if (mbrs[idx] != NULL) {
            if (strcmp(mbrs[idx]->key, mbr->key) == 0) {
                // This key already exists...
                return false;
            }
            idx = (idx + quad_probe(idx)) % len;
            continue;
        }
        break;
    }

    mbrs[idx] = mbr;
    return true;
}


// Might revisit this if we use a hash function that preserves ordering..
static bool resize_json_object(json_object *obj) {
    const size_t new_capacity = obj->capacity * 2;
    json_member **new_arr = calloc(new_capacity, sizeof(json_member*));
    if (new_arr == NULL) return false;

    // Re-hash all the elements into the new array
    for (size_t i = 0; i < obj->capacity; i++) {
        if (obj->members[i] == NULL) continue;

        const size_t idx = hash_key(obj->members[i]->key) % new_capacity;
        if (new_arr[idx] != NULL) {
            if (!handle_collision((const json_member**)new_arr,
                                  new_capacity, idx, obj->members[i]))
                return false;
        } else {
            new_arr[idx] = obj->members[i];
        }
    }

    free(obj->members);
    obj->members = new_arr;
    return true;
}

json_member *new_json_member(const char *key, json_value *val) {
    json_member *new_mbr = NULL;
    new_mbr = calloc(1, sizeof(json_member));
    if (new_mbr == NULL) {
        // FATAL
        exit(EXIT_FAILURE);
    }
    new_mbr->key = key;
    new_mbr->val = val;

    return new_mbr;
}

json_array *new_json_array(void) {
    json_array *new_arr = NULL;
    new_arr = calloc(1, sizeof(json_array));
    if (new_arr == NULL) {
        // FATAL
        exit(EXIT_FAILURE);
    }

    new_arr->values = calloc(INIT_CAPACITY, sizeof(json_value*));
    if (new_arr->values == NULL) {
        // FATAL
        exit(EXIT_FAILURE);
    }

    new_arr->capacity = INIT_CAPACITY;
    return new_arr;
}


json_object *new_json_object(void) {
    json_object *new_obj = NULL;

    new_obj = calloc(1, sizeof(json_object));
    if (new_obj == NULL) {
        return NULL;
    }
    new_obj->members = calloc(INIT_CAPACITY, sizeof(json_member*));
    if (new_obj->members == NULL) {
        free(new_obj);
        return NULL;
    }

    new_obj->capacity = INIT_CAPACITY;
    return new_obj;
}


void dispose_json_array(json_array *arr) {
    if (!arr) return;
    if (arr->values == NULL) {
        free(arr);
        return;
    }

    for (size_t i = 0; i < arr->size; i++) {
        assert(arr->values != NULL);
        assert(arr->values[i] != NULL);

        dispose_json_value(arr->values[i]);
        arr->values[i] = NULL;
    }

    free(arr->values);
    free(arr);
}


void dispose_json_object(json_object *obj) {
    if (!obj) return;

    for (size_t i = 0; i < obj->capacity; i++) {
        if (obj->members[i] == NULL) continue;

        dispose_json_member(obj->members[i]);
        obj->members[i] = NULL;
    }

    free(obj->members);
    free(obj);
}

void dispose_json_value(json_value *val) {
    if (!val) return;
    switch (val->type) {
    case JSON_OBJECT:
        dispose_json_object(val->obj);
        break;
    case JSON_ARRAY:
        dispose_json_array(val->arr);
        break;
    case JSON_STRING:
        free(val->string);
        break;
    case JSON_BOOL:
    /* FALLTHRU */
    case JSON_NULL:
    /* FALLTHRU */
    case JSON_NUMBER:
    /* FALLTHRU */
        break;
    }
    free(val);
}

void dispose_json_member(json_member *mbr) {
    if (!mbr) return;
    free((void*)mbr->key);
    mbr->key = NULL;
    dispose_json_value(mbr->val);
    mbr->val = NULL;
    free(mbr);
}

bool add_value(json_array *arr, const json_value *val) {
    if (!arr || !val) return false;

    // Resize array if needed
    if (arr->capacity == arr->size) {
        size_t new_cap = arr->capacity * 2;

        json_value **tmp = realloc(arr->values, sizeof(json_value*) * new_cap);
        if (tmp == NULL) {
            // FATAL
            exit(EXIT_FAILURE);
        }
        arr->values = tmp;
        arr->capacity = new_cap;
    }

    arr->values[arr->size++] = (json_value*)val;
    return true;
}


/* IMPORANT: This does not copy `mem`, so you cannot free the json_member
             before the json_object is destroyed. */
bool add_member(json_object *const obj, const json_member *mbr) {
    if (!obj || !mbr) return false;

    // Check if underlying array needs to grow
    if ((obj->capacity * LOAD_FACTOR) / 100 <= obj->size) {
        resize_json_object(obj);
    }

    const size_t idx = hash_key(mbr->key) % obj->capacity;
    if (obj->members[idx] != NULL) {
        if (!handle_collision((const json_member**)obj->members,
                              obj->capacity, idx, mbr))
            return false;
    } else {
        obj->members[idx] = (json_member*)mbr;
    }

    obj->size++;
    return true;
}


size_t size_json_object(const json_object *obj) {
    if (!obj) return 0;
    return obj->size;
}

size_t size_json_array(const json_array *arr) {
    if (!arr) return 0;
    return arr->size;
}

void print_json_value(const json_value *const val) {
    print_value_indent(val, 0, false);
}

void print_json_object(const json_object *obj) {
    print_object_indent(obj, 0, false);
}

void print_json_array(const json_array *arr) {
    print_array_indent(arr, 0, false);
}


static void print_array_indent(const json_array *const arr, size_t level, bool ismbr) {
    char *indent = NULL;
    char *end_indt = NULL;

    if (ismbr) {
        indent = "";
        SET_INDENT(end_indt, (level));
    } else {
        SET_INDENT(indent, level);
        SET_INDENT(end_indt, level);
    }

    if (arr->size == 0) {
        fprintf(stderr, "%s[]", indent);
        return;
    }

    fprintf(stderr, "%s[\n", indent);
    for (size_t i = 0; i < arr->size; i++) {
        //fprintf(stderr, "%s", indent);
        print_value_indent(arr->values[i], level + 2, false);
        if ((i + 1) != arr->size) {
            fprintf(stderr, ",\n");
        } else {
            fprintf(stderr, "\n");
        }
    }
    fprintf(stderr, "%s]", end_indt);
}


static void print_object_indent(const json_object *obj, size_t level, bool ismbr) {
    char *indent = NULL;
    char *end_idnt = NULL;

    if (ismbr) {
        indent = "";
        SET_INDENT(end_idnt, level);
    } else {
        SET_INDENT(indent, level);
        SET_INDENT(end_idnt, level);
    }

    size_t printed = 0;

    fprintf(stderr, "%s{\n", indent);
    for (size_t i = 0; i < obj->capacity; i++) {
        if (obj->members[i] == NULL) continue;
        print_member_indent(obj->members[i], level + 2);
        printed++;
        if (printed == obj->size) {
            fprintf(stderr, "\n");
            break;
        } else {
            fprintf(stderr, ",\n");
        }
    }
    fprintf(stderr, "%s}", end_idnt);
}


static void print_value_indent(const json_value *val, size_t level, bool ismbr) {
    char *indent = NULL;

    if (ismbr) {
        indent = "";
    } else {
        SET_INDENT(indent, level);
    }

    switch(val->type) {
    case JSON_NUMBER:
        fprintf(stderr, "%s"LITERAL_COLOR("%g"), indent, val->number);
        break;
    case JSON_STRING:
        fprintf(stderr, "%s"STRING_COLOR("\"%s\""), indent, val->string);
        break;
    case JSON_BOOL:
        fprintf(stderr, "%s"LITERAL_COLOR("%s"), indent, (val->boolean) ? "true" : "false");
        break;
    case JSON_NULL:
        fprintf(stderr, "%s"LITERAL_COLOR("null"), indent);
        break;
    case JSON_ARRAY:
        print_array_indent(val->arr, level, ismbr);
        break;
    case JSON_OBJECT:
        print_object_indent(val->obj, level, ismbr);
        break;
    default:
        fprintf(stderr, LITERAL_COLOR("<unknown type>"));
    }
}


static void print_member_indent(const json_member *mbr, size_t level) {
    char *indent = NULL;
    SET_INDENT(indent, level);

    fprintf(stderr, "%s"KEY_COLOR("\"%s\"")": ", indent, mbr->key);
    print_value_indent(mbr->val, level, true);
}


json_value *json_arr(json_array *arr) {
    json_value *new_val = calloc(1, sizeof(json_value));
    if (!new_val) {
        // FATAL
        exit(EXIT_FAILURE);
    }

    new_val->type = JSON_ARRAY;
    new_val->arr = arr;

    return new_val;
}

json_value *json_obj(json_object *obj) {
    json_value *new_val = calloc(1, sizeof(json_value));
    if (!new_val) {
        // FATAL
        exit(EXIT_FAILURE);
    }

    new_val->type = JSON_OBJECT;
    new_val->obj = obj;

    return new_val;
}

json_value *json_null(void) {
    json_value *new_val = calloc(1, sizeof(json_value));
    if (!new_val) {
        // FATAL
        exit(EXIT_FAILURE);
    }

    new_val->type = JSON_NULL;
    return new_val;
}

json_value *json_str(const char *str) {
    json_value *new_val = calloc(1, sizeof(json_value));
    if (!new_val) {
        // FATAL
        exit(EXIT_FAILURE);
    }

    new_val->type = JSON_STRING;
    new_val->string = (char*)str;

    return new_val;
}

json_value *json_bool(bool boolean) {
    json_value *new_val = calloc(1, sizeof(json_value));
    if (!new_val) {
        // FATAL
        exit(EXIT_FAILURE);
    }

    new_val->type = JSON_BOOL;
    new_val->boolean = boolean;

    return new_val;
}

json_value *json_num(double num) {
    json_value *new_val = calloc(1, sizeof(json_value));
    if (!new_val) {
        // FATAL
        exit(EXIT_FAILURE);
    }

    new_val->type = JSON_NUMBER;
    new_val->number = num;

    return new_val;
}


const json_value *get_value_from_path(const char *path, json_value *val) {
    // Split `path` on '/' into `members`
    char **members = NULL;

    // TODO

    return NULL;
}


const char *get_key_from_member(const json_member *mbr) {
    return mbr->key;
}


/* Appends `str2` to `str1` */
static char *concat(char *str1, char *str2) {
    if (str2 == NULL) {
        return str1;
    }

    size_t len1 = (str1) ? strlen(str1) : 0;
    size_t len2 = strlen(str2);

    char *buf = realloc(str1, (len1 + len2 + 1) * sizeof(char));
    assert(buf != NULL);
    if (str1 == NULL) {
        buf[0] = '\0';
    }

    strcat(buf, str2);

    return buf;
}


char *stringify_json_member(const json_member *mbr) {
    char *str = calloc(strlen(mbr->key) + 4, sizeof(char));
    assert(str != NULL);
    sprintf(str, "\"%s\":", mbr->key);

    char *tmp = stringify_json_value(mbr->val);
    str = concat(str, tmp);
    free(tmp);
    return str;
}


char *stringify_json_object(const json_object *obj) {
    char *str = calloc(2, sizeof(char));
    strcpy(str, "{");

    size_t seen = 0;

    for (size_t i = 0; i < obj->capacity; i++) {
        if (obj->members[i] == NULL) continue;
        char *tmp = stringify_json_member(obj->members[i]);
        str = concat(str, tmp);
        free(tmp);
        seen++;
        if (seen != obj->size) {
            str = concat(str, ",");
        }
    }

    return concat(str, "}");
}


char *stringify_json_array(const json_array *arr) {
    char *str = calloc(2, sizeof(char));
    strcpy(str, "[");

    for (size_t i = 0; i < arr->size; i++) {
        if (arr->values[i] == NULL) continue;
        char *tmp = stringify_json_value(arr->values[i]);
        str = concat(str, tmp);
        free(tmp);

        if ((i + 1) != arr->size) {
            str = concat(str, ",");
        }
    }

    return concat(str, "]");
}


char *stringify_json_value(const json_value *val) {
    if (val == NULL) return NULL;

    char *str = NULL;

    switch (val->type) {
    case JSON_OBJECT:
        str = stringify_json_object(val->obj);
        break;
    case JSON_ARRAY:
        str = stringify_json_array(val->arr);
        break;
    case JSON_STRING:
        str = concat(str, "\"");
        str = concat(str, val->string);
        str = concat(str, "\"");
        break;
    case JSON_NUMBER:
        {   // TODO
        char buf[64] = {0};
        strfromd(buf, 63, "%g", val->number);
        str = concat(str, buf);
        }
        break;
    case JSON_BOOL:
        str = concat(str, (val->boolean) ? "true" : "false");
        break;
    case JSON_NULL:
        str = concat(str, "null");
        break;
    default:
    // unknown type
        str = concat(str, "<undefined>");
        break;
    }
    return str;
}
