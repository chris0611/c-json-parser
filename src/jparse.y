%define parse.error verbose

%{
#include <stdio.h>
#include <stdlib.h>
#include <stdbool.h>
#include <string.h>
#include <errno.h>
#include <assert.h>

#include "jsonobject/jsonobject.h"

#define INIT_SCOPE_CAP (32)

struct lex_ctx;

int yylex(struct lex_ctx*);
void yyerror(struct lex_ctx*, const char*);
enum json_token lex(struct lex_ctx*);
%}

%union{
    double numval;
    char *strval;
    struct json_value* val;
    struct json_array* arr;
    struct json_object *obj;
    struct json_member *mbr;
    bool boolean;
}

%param{ struct lex_ctx *ctx }

%code{

typedef struct node {
    json_member *mbr;

    struct node *next;
} node;

typedef struct scope {
    json_object *obj;       // Object that owns this scope
    
    // Linked list of created members (key-value pairs) within current scope
    node *head;             // First node in list
    node *tail;             // Last node in list
    size_t num_mbrs;

    json_array **arrs;      // Stack keeping track of arrays in scope
    size_t num_arrs;
} scope;

struct lex_ctx {
    json_value *top_val;    // Top level value, result is stored here

    scope *scopes;          // Scopes (keeps track of nested objects)
    size_t num_scopes;      // Number of scopes currently
    size_t scope_cap;       // Capacity; number or scopes with space allocated

    // File location
    size_t line;
    size_t col;
    const char *cursor;
    const char *filename;
};

scope *get_scope(struct lex_ctx *ctx) {
    assert(ctx != NULL);
    assert(ctx->scopes != NULL);
    scope *current = &ctx->scopes[ctx->num_scopes-1];
    assert(current != NULL);
    return current;
}

void begin_scope(struct lex_ctx *ctx) {
    // Allocate more space
    if (ctx->num_scopes == ctx->scope_cap) {
        // push a new scope onto internal data structure
        const size_t new_cap = (ctx->scope_cap) 
                             ? (ctx->scope_cap * 2) : (INIT_SCOPE_CAP);
        scope *tmp = realloc(ctx->scopes, new_cap * sizeof(scope));
        if (tmp == NULL) {
            // FATAL
            exit(EXIT_FAILURE);
        }
        ctx->scope_cap = new_cap;
        ctx->scopes = tmp;
    }

    json_object *new_obj = new_json_object();
    assert(ctx->scopes != NULL);

    ctx->scopes[ctx->num_scopes] = (scope){
        .obj = new_obj,
        .num_mbrs = 0,
        .head = NULL,
        .tail = NULL,
        .arrs = NULL,
        .num_arrs = 0
    };

    ctx->num_scopes++;
}

json_object *end_scope(struct lex_ctx *ctx) {
    scope *curr = get_scope(ctx);

    json_object *parsed_obj = curr->obj;
    node *tmp = curr->head;

    do {
        add_member(parsed_obj, tmp->mbr);
        node *next = tmp->next;
        free(tmp);
        tmp = next;
    } while (tmp != NULL);

    // pop scope from internal data structure
    // TODO: should also check if we can free mem,
    // to avoid memory leak
    ctx->num_scopes--;
    return parsed_obj;
}

// TODO: ERROR HANDLING, IMPROVE ALLOCATION ALGORITHM
void begin_arr(struct lex_ctx *ctx) {
    json_array *new_arr = new_json_array();
    assert(new_arr != NULL);

    scope *curr = get_scope(ctx);

    if (curr->arrs == NULL) {
        curr->arrs = calloc(1, sizeof(json_array*));
        assert(curr->arrs != NULL);

        curr->arrs[0] = new_arr;
        curr->num_arrs = 1;
        return;
    }

    const size_t new_size = curr->num_arrs + 1;
    json_array **tmp = realloc(curr->arrs, sizeof(json_array*) * new_size);
    assert(tmp != NULL);

    tmp[new_size-1] = new_arr;
    curr->num_arrs = new_size;
    curr->arrs = tmp;
}

json_array *end_arr(struct lex_ctx *ctx) {
    scope *curr = get_scope(ctx);

    assert(curr->arrs != NULL);
    json_array *ret = curr->arrs[curr->num_arrs-1];
    assert(ret != NULL);

    if (curr->num_arrs == 1) {
        // special case, free
        curr->num_arrs = 0;
        free(curr->arrs);
        curr->arrs = NULL;
        return ret;
    }

    size_t new_size = curr->num_arrs - 1;

    // TODO: check for errs
    json_array **tmp = realloc(curr->arrs, (sizeof(json_array*) * new_size));
    assert(tmp != NULL);

    curr->arrs = tmp;
    curr->num_arrs--;

    return ret;
}

void push_value(struct lex_ctx *ctx, json_value* val) {
    assert(val != NULL);

    scope *curr = get_scope(ctx);
    assert(curr->arrs != NULL);

    json_array *curr_arr =  curr->arrs[curr->num_arrs-1];
    assert(curr_arr != NULL);

    add_value(curr_arr, val);
}

void push_member(struct lex_ctx *ctx, json_member *mbr) {
    assert(mbr != NULL);
    // Add member to linked list
    // TODO: Error checking
    scope *curr = get_scope(ctx);

    node *new_node = calloc(1, sizeof(node));
    assert(new_node != NULL);
    new_node->next = NULL;
    new_node->mbr = mbr;

    if (curr->head == NULL) {
        curr->head = new_node;
        curr->tail = new_node;
        curr->num_mbrs++;
        return;
    }

    // Add after last node
    node *last = curr->tail;
    last->next = new_node;
    curr->tail = new_node;
    curr->num_mbrs++;
}

} // %code

%token STRING
%token NUMBER
%token TOK_TRUE "true"
%token TOK_FALSE "false"
%token TOK_NULL "null"


%type<val>      value element
%type<obj>      object
%type<arr>      array
%type<mbr>      member

%%
json:       element { ctx->top_val = $1; }
|           %empty  { ctx->top_val = NULL; };
value:      object  { $$ = json_obj($1); }
|           array   { $$ = json_arr($1); }
|           STRING  { $$ = json_str($1); }
|           NUMBER  { $$ = json_num($1); }
|           "true"  { $$ = json_bool(true); }
|           "false" { $$ = json_bool(false); }
|           "null"  { $$ = json_null(); };
object:     '{' '}' { $$ = new_json_object(); }
|           '{' { begin_scope(ctx); } members '}' { $$ = end_scope(ctx); };
members:    member  { push_member(ctx, $1); }
|           member  { push_member(ctx, $1); } ',' members;
member:     STRING ':' element { $$ = new_json_member($1, $3); };
array:      '[' ']' { $$ = new_json_array(); }
|           '[' { begin_arr(ctx); } elements ']' { $$ = end_arr(ctx); };
elements:   element { push_value(ctx, $1); }
|           element { push_value(ctx, $1); } ',' elements;
element:    value   { $$ = $1; };
%%


int yylex(struct lex_ctx *ctx)
{
    const char *anchor, *YYMARKER;
start_lex:
    anchor = ctx->cursor;
%{
    /* re2c */
    re2c:yyfill:enable = 0;
    re2c:define:YYCURSOR = ctx->cursor;
    re2c:define:YYCTYPE = char;

    // Whitespace:
    '\000'              { return YYEOF; }
    "\r\n" | [\r\n]     { ctx->line++; ctx->col = 1; goto start_lex; }
    [\t\v\b\f ]         { ctx->col++; goto start_lex; }

    // Numbers (floats and ints):
    frac  = [0-9]* "." [0-9]+ | [0-9]+ ".";
    exp   = 'e' [+-]? [0-9]+;
    float = frac exp? | [0-9]+ exp;
    [1-9][0-9]* | float { ctx->col += (ctx->cursor-anchor);
                          yylval.numval = strtod(anchor, NULL);
                          return NUMBER; }

    // Keywords:
    "null"              { ctx->col += 4; return TOK_NULL; }
    "true"              { ctx->col += 4; return TOK_TRUE; }
    "false"             { ctx->col += 5; return TOK_FALSE; }

    // Strings:
    "\"" [^\000"]* "\""     { ctx->col += (ctx->cursor-anchor);
                          yylval.strval = strndup(anchor+1, ctx->cursor-anchor-2);
                          return STRING; }

    // Anything else (returns the current char):
    .       { ctx->col++; return *(ctx->cursor-1); }
%}
}

void yyerror(struct lex_ctx *ctx, const char *msg)
{
    fprintf(stderr, "\033[1m%s:%zu:%zu:\033[m \033[1;91merror:\033[m %s\n",
            ctx->filename, ctx->line, ctx->col, msg);

    exit(EXIT_FAILURE); // All errors are currently fatal :/
}


static char *read_into_str(FILE *stream, size_t *len)
{
    fseek(stream, 0, SEEK_END);
    *len = ftell(stream);
    fseek(stream, 0, SEEK_SET);

    char *buffer = malloc((*len+1) * sizeof(char));
    size_t read = fread(buffer, sizeof(char), *len, stream);
    buffer[read] = '\0';

    if (read < *len) {
        *len = read;
        char *tmp = realloc(buffer, (read+1) * sizeof(char));
        if (!tmp) { free(buffer); return NULL; }
        buffer = tmp;
    }

    return buffer;
}


int main(int argc, const char *argv[])
{
    // DEBUG
    //yydebug = 1;

    if (argc != 2) {
        fprintf(stderr, "Usage: %s <json-file>\n", argv[0]);
        return EXIT_FAILURE;
    }

    int ret_val = 0;

    FILE *json_file = fopen(argv[1], "r");
    if (!json_file) {
        fprintf(stderr, "Failed to open file: %s, error: %s\n",
                argv[1], strerror(errno));
        return EXIT_FAILURE;
    }

    size_t buflen = 0;
    char *buffer = read_into_str(json_file, &buflen);

    // Setting up lexing context
    struct lex_ctx ctx = {
        .line = 1,
        .col = 1,
        .filename = argv[1],
        .cursor = buffer,
        .top_val = NULL,
        .scopes = calloc(1, sizeof(scope)),
        .num_scopes = 1,
        .scope_cap = 1
    };
    
    if (0 > yyparse(&ctx)) {
        fprintf(stderr, "parsing failed\n");
        ret_val = 1;
        goto cleanup;
    }

    fprintf(stderr, "Finished parsing: %s\n", argv[1]);

    if (ctx.top_val == NULL) {
        fprintf(stderr, "EMPTY\n");
        goto cleanup;
    }

    print_json_value(ctx.top_val);
    fprintf(stderr, "\n");

    dispose_json_value(ctx.top_val);

cleanup:
    free(ctx.scopes);
    free(buffer);
    fclose(json_file);
    return ret_val;
}
