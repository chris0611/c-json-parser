#include "jsonobject.h"

#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <assert.h>
#include <string.h>

#define LOAD_FACTOR (60)        // Grow Hashmap if 60% filled
#define INIT_CAPACITY (64)      // Initial capacity of array
#define DOWNSIZE_LIMIT (25)     // Shrink ifvless than 25% filled


#define SET_INDENT(ptr, level)                      \
    do {                                            \
        char *tmp = (ptr);                          \
        tmp = alloca((level + 1) * sizeof(char));   \
        memset(tmp, ' ', level); tmp[level] = '\0'; \
        ptr = tmp;                                  \
    } while (0)

#define ORANGE(str) "\033[38;5;132m" str "\033[m"
#define PURPLE(str) "\033[38;5;108m" str "\033[m"
#define YELLOW(str) "\033[38;5;145m" str "\033[m"

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
static void print_array_indent(json_array *arr, size_t level, bool ismbr);
static void print_object_indent(json_object *obj, size_t level, bool ismbr);
static void print_value_indent(json_value *val, size_t level, bool ismbr);
static void print_member_indent(json_member *mbr, size_t level);


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


static void handle_collision(const json_member ** mbrs, size_t len,
                             size_t start_idx, const json_member *mbr) {
    // Do something smarter than linear probing?
    size_t idx = start_idx;

    // Should _always_ have an open spot
    while (true) {
        if (mbrs[idx] != NULL) {
            idx = (idx + quad_probe(idx)) % len;
            continue;
        }
        break;
    }

    mbrs[idx] = mbr;
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
            handle_collision((const json_member**)new_arr,
                             new_capacity, idx, obj->members[i]);
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
        handle_collision((const json_member**)obj->members,
                         obj->capacity, idx, mbr);
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

void print_json_value(json_value *val) {
    print_value_indent(val, 0, false);
}

void print_json_object(json_object *obj) {
    print_object_indent(obj, 0, false);
}

void print_json_array(json_array *arr) {
    print_array_indent(arr, 0, false);
}


static void print_array_indent(json_array *arr, size_t level, bool ismbr) {
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


static void print_object_indent(json_object *obj, size_t level, bool ismbr) {
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


static void print_value_indent(json_value *val, size_t level, bool ismbr) {
    char *indent = NULL;

    if (ismbr) {
        indent = "";
    } else {
        SET_INDENT(indent, level);
    }

    switch(val->type) {
    case JSON_NUMBER:
        fprintf(stderr, "%s"PURPLE("%f"), indent, val->number);
        break;
    case JSON_STRING:
        fprintf(stderr, "%s"YELLOW("\"%s\""), indent, val->string);
        break;
    case JSON_BOOL:
        fprintf(stderr, "%s"PURPLE("%s"), indent, (val->boolean) ? "true" : "false");
        break;
    case JSON_NULL:
        fprintf(stderr, "%s"PURPLE("null"), indent);
        break;
    case JSON_ARRAY:
        print_array_indent(val->arr, level, ismbr);
        break;
    case JSON_OBJECT:
        print_object_indent(val->obj, level, ismbr);
        break;
    default:
        fprintf(stderr, PURPLE("<unknown type>"));
    }
}


static void print_member_indent(json_member *mbr, size_t level) {
    char *indent = NULL;
    SET_INDENT(indent, level);

    fprintf(stderr, "%s"ORANGE("\"%s\"")": ", indent, mbr->key);
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