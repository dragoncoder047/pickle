#ifdef __cplusplus
extern "C" {
#endif

#include <stdlib.h>
#include <stdarg.h>
#include <stdio.h>
#include <string.h>
#include <ctype.h>
#pragma GCC optimize ("Os")
#if !defined(bool) && !defined(__cplusplus)
typedef int bool;
#define true 1
#define false 0
#endif
#define streq(a, b) (!strcmp((a), (b)))
#define IF_NULL_RETURN(x) if ((x) == NULL) return
#ifndef fputchar
#define fputchar(s, c) fprintf(s, "%c", c)
#endif

#define PIK_DEBUG
#define PIK_TEST
#define PIK_INCLUDE_FILE_LOCATIONS

#ifdef PIK_DEBUG
#define PIK_DEBUG_PRINTF printf
#define PIK_DEBUG_ASSERT(cond, should) __PIK_DEBUG_ASSERT_INNER(cond, should, #cond, __FILE__, __LINE__)
void __PIK_DEBUG_ASSERT_INNER(bool cond, const char* should, const char* condstr, const char* filename, size_t line) {
    printf("[%s:%zu] Assertion %s: %s\n", filename, line, cond ? "succeeded" : "failed", condstr);
    if (cond) return;
    printf("%s\nAbort.", should);
    exit(70);
}
#else
#define PIK_DEBUG_PRINTF(...)
#define PIK_DEBUG_ASSERT(...)
#endif

// Section: Enums

enum type {
    //                     Contents
    CONS,               // car         | cdr       | (empty)
    SYMBOL,             // char*       | (empty)   | (empty)
    STRING,             // char*       | (empty)   | (empty)
    ERROR,              // char*       | (empty)   | (empty)
    INTEGER,            // int64_t............     | (empty)
    BOOLEAN,            // uint8_t     | (empty)   | (empty)
    FLOAT,              // double.............     | (empty)
    COMPLEX,            // float       | float     | (empty)
    RATIONAL,           // float       | float     | (empty)
    BUILTIN_FUNCTION,   // char*       | func      | (empty)
    STREAM,             // char*       | FILE*     | (empty)

    // Complex types
    LIST,               // item**s     | len       | (empty)
    MAP,                // item**s     | len       | (empty)
    KV_PAIR,            // key         | value     | (empty)
    CLASS,              // parent**s   | len       | scope    -- properties are namespace
    USER_FUNCTION,      // name        | body      | scope    -- properties are arguments since a map preserves order

    // Internal types (mostly AST nodes)
    ARGUMENT_ENTRY,     // name        | default   | (empty)
    OPERATOR,           // char*       | (empty)   | (empty)
    GETVAR,             // char*       | (empty)   | (empty)
    EXPRESSION,         // item**s     | len       | (empty)
    BLOCK,              // item**s     | len       | (empty)
    LIST_LITERAL,       // item**s     | len       | (empty)
    SCOPE,              // operator**s | len       | result   -- bindings are on properties, parent scope is class
    BINDINGS_LIST,      // item**s     | len       | (empty)
    BINDING,            // char*       | value     | (empty)
    BOUND_METHOD        // method      | self      | (empty)
};

enum flag {
    MARKBIT = 1,
    FINALIZED = 2,
    ERROR_HAS_BEEN_CAUGHT = 4,
    FUNCTION_IS_TCO = 4,
    FUNCTION_IS_MACRO = 8
};

enum resultcode {
    ROK,
    RERROR,
    RBREAK,
    RCONTINUE,
    RRETURN
};

// Section: Typedefs

typedef struct pickle* pickle_t;

typedef struct pik_object* pik_object_t;
typedef int (*pik_func)(pickle_t vm, pik_object_t self, pik_object_t args, pik_object_t scope);
struct pik_object {
    // Basic information about the object
    uint16_t type;
    pik_object_t next_object;
    uint16_t subtype;
    // Garbage collection and finalization flags
    uint16_t flags;
    size_t refcnt;
#ifdef PIK_INCLUDE_FILE_LOCATIONS
    struct {
        uint32_t line;
        char* sourcefile;
    };
#endif
    // OOP stuff
    pik_object_t classes;
    pik_object_t properties;
    // Payload
    union {
        int64_t integer;
        double floatnum;
        struct {
            // Cell 1
            union {
                pik_object_t cell1, car, item, key, parents, symbol, element, bound_method;
                pik_object_t* items;
                pik_object_t* operators;
                char* chars;
                char* message;
                char* name;
                float real;
                int32_t numerator;
                uint8_t boolean;
            };
            // Cell 2
            union {
                pik_object_t cell2, cdr, value, body, def, bound_self;
                uint32_t len;
                float imag;
                uint32_t denominator;
                FILE* stream;
                pik_func function;
            };
            // Cell 3
            union {
                pik_object_t cell3, scope, result;
            };
        };
    };
};

struct pickle {
    pik_object_t first_object;
    size_t num_objects;
    pik_object_t global_scope;
    pik_object_t dollar_function;
    pik_object_t type_protos;
};

// Section: Forward references
void pik_decref(pickle_t vm, pik_object_t object);
static int eval_block(pickle_t vm, pik_object_t self, pik_object_t block, pik_object_t args, pik_object_t scope);
static int call(pickle_t vm, pik_object_t func, pik_object_t self, pik_object_t args, pik_object_t scope);
int pik_eval(pickle_t vm, pik_object_t self, pik_object_t x, pik_object_t args, pik_object_t scope);
static void register_stdlib(pickle_t vm);

// Section: Garbage Collector

// #define PIK_DOCALLBACK(vm, name, ...) \
// if (vm->callbacks.name) { \
//     vm->callbacks.name(vm, __VA_ARGS__);\
// }

static const char* type_name(uint16_t type) {
    switch (type) {
        case CONS:              return "object"; // generic object
        case SYMBOL:            return "symbol";
        case STRING:            return "string";
        case ERROR:             return "error";
        case INTEGER:           return "integer";
        case BOOLEAN:           return "boolean";
        case FLOAT:             return "float";
        case COMPLEX:           return "complex";
        case RATIONAL:          return "rational";
        case BUILTIN_FUNCTION:  return "builtin_function";
        case STREAM:            return "stream";
        case LIST:              return "list";
        case KV_PAIR:           return "kv_pair";
        case MAP:               return "map";
        case CLASS:             return "class";
        case USER_FUNCTION:     return "function";
        case ARGUMENT_ENTRY:    return "argument_entry";
        case GETVAR:            return "getvar";
        case OPERATOR:          return "operator";
        case EXPRESSION:        return "expression";
        case BLOCK:             return "block";
        case LIST_LITERAL:      return "list_literal";
        case SCOPE:             return "scope";
        case BINDINGS_LIST:     return "bindings_list";
        case BINDING:           return "binding";
        case BOUND_METHOD:      return "bound_method";
    }
    return "object";
}

static pik_object_t alloc_object(pickle_t vm, uint16_t type, uint16_t subtype) {
    pik_object_t object = vm->first_object;
    while (object != NULL) {
        if (object->refcnt == 0) {
            PIK_DEBUG_PRINTF("Reusing garbage ");
            goto got_unused;
        }
        object = object->next_object;
    }
    PIK_DEBUG_PRINTF("Allocating new memory ");
    object = (pik_object_t)calloc(1, sizeof(struct pik_object));
    object->next_object = vm->first_object;
    vm->first_object = object;
    vm->num_objects++;
    got_unused:
    PIK_DEBUG_PRINTF("for a %s at %p\n", type_name(type), (void*)object);
    object->type = type;
    object->subtype = subtype;
    object->refcnt = 1;
    object->properties = NULL;
    object->flags = 0;
    return object;
}

inline void pik_incref(pik_object_t object) {
    IF_NULL_RETURN(object);
    object->refcnt++;
    PIK_DEBUG_PRINTF("%s at %p got a new reference (now have %zu)\n", type_name(object->type), (void*)object, object->refcnt);
}

enum _type_fields {
    CELL1_EMPTY   = 0b000000,
    CELL1_CHARS   = 0b000001,
    CELL1_OBJECT  = 0b000010,
    CELL1_OBJECTS = 0b000011,
    CELL1_MASK    = 0b000011,
    CELL2_EMPTY   = 0b000000,
    CELL2_FILE    = 0b000100,
    CELL2_OBJECT  = 0b001000,
    CELL2_MASK    = 0b001100,
    CELL3_EMPTY   = 0b000000,
    CELL3_OBJECT  = 0b010000,
    CELL3_MASK    = 0b110000
};

static int type_info(uint16_t type) {
    switch (type) {
        case CONS:              return CELL1_OBJECT  | CELL2_OBJECT | CELL3_EMPTY;
        case SYMBOL:            return CELL1_CHARS   | CELL2_EMPTY  | CELL3_EMPTY;
        case STRING:            return CELL1_CHARS   | CELL2_EMPTY  | CELL3_EMPTY;
        case ERROR:             return CELL1_CHARS   | CELL2_EMPTY  | CELL3_EMPTY;
        case INTEGER:           return CELL1_EMPTY   | CELL2_EMPTY  | CELL3_EMPTY;
        case BOOLEAN:           return CELL1_EMPTY   | CELL2_EMPTY  | CELL3_EMPTY;
        case FLOAT:             return CELL1_EMPTY   | CELL2_EMPTY  | CELL3_EMPTY;
        case COMPLEX:           return CELL1_EMPTY   | CELL2_EMPTY  | CELL3_EMPTY;
        case RATIONAL:          return CELL1_EMPTY   | CELL2_EMPTY  | CELL3_EMPTY;
        case BUILTIN_FUNCTION:  return CELL1_CHARS   | CELL2_EMPTY  | CELL3_EMPTY;
        case STREAM:            return CELL1_CHARS   | CELL2_FILE   | CELL3_EMPTY;
        case LIST:              return CELL1_OBJECTS | CELL2_EMPTY  | CELL3_EMPTY;
        case KV_PAIR:           return CELL1_OBJECT  | CELL2_OBJECT | CELL3_EMPTY;
        case MAP:               return CELL1_OBJECTS | CELL2_EMPTY  | CELL3_EMPTY;
        case CLASS:             return CELL1_OBJECTS | CELL2_EMPTY  | CELL3_OBJECT;
        case USER_FUNCTION:     return CELL1_CHARS   | CELL2_OBJECT | CELL3_OBJECT;
        case ARGUMENT_ENTRY:    return CELL1_CHARS   | CELL2_OBJECT | CELL3_EMPTY;
        case GETVAR:            return CELL1_CHARS   | CELL2_EMPTY  | CELL3_EMPTY;
        case OPERATOR:          return CELL1_CHARS   | CELL2_EMPTY  | CELL3_EMPTY;
        case EXPRESSION:        return CELL1_OBJECTS | CELL2_EMPTY  | CELL3_EMPTY;
        case BLOCK:             return CELL1_OBJECTS | CELL2_EMPTY  | CELL3_EMPTY;
        case LIST_LITERAL:      return CELL1_OBJECTS | CELL2_EMPTY  | CELL3_EMPTY;
        case SCOPE:             return CELL1_OBJECTS | CELL2_EMPTY  | CELL3_OBJECT;
        case BINDINGS_LIST:     return CELL1_OBJECTS | CELL2_EMPTY  | CELL3_EMPTY;
        case BINDING:           return CELL1_CHARS   | CELL2_OBJECT | CELL3_EMPTY;
        case BOUND_METHOD:      return CELL1_OBJECT  | CELL2_OBJECT | CELL3_EMPTY;
    }
    return 0; // Satisfy compiler
}

static void finalize(pickle_t vm, pik_object_t object) {
    IF_NULL_RETURN(object);
    if (object->flags & FINALIZED) {
        PIK_DEBUG_PRINTF("Already finalized %s at %p\n", type_name(object->type), (void*)object);
        return;
    }
    PIK_DEBUG_PRINTF("Finalizing %s at %p\n", type_name(object->type), (void*)object);
    // Free object-specific stuff
    switch (type_info(object->type) & CELL1_MASK) {
        case CELL1_EMPTY: break;
        case CELL1_CHARS: free(object->chars); object->chars = NULL; break;
        case CELL1_OBJECT: pik_decref(vm, object->cell1); object->cell1 = NULL; break;
    }
    switch (type_info(object->type) & CELL2_MASK) {
        case CELL2_EMPTY: break;
        case CELL2_FILE: fclose(object->stream); object->stream = NULL; break;
        case CELL2_OBJECT: pik_decref(vm, object->cell2); object->cell2 = NULL; break;
    }
    switch (type_info(object->type) & CELL3_MASK) {
        case CELL3_EMPTY: break;
        case CELL3_OBJECT: pik_decref(vm, object->cell3); object->cell3 = NULL; break;
    }
    object->integer = 0; // Clear all
    // Free everything else
    object->flags = FINALIZED;
    pik_decref(vm, object->classes);
    pik_decref(vm, object->properties);
    object->properties = object->classes = NULL;
}

void pik_decref(pickle_t vm, pik_object_t object) {
    IF_NULL_RETURN(object);
    object->refcnt--;
    if (object->refcnt <= 0) {
        PIK_DEBUG_PRINTF("%s at %p lost all references, finalizing\n", type_name(object->type), (void*)object);
        // Free it now, no other references
        finalize(vm, object);
        // Unmark it so it will be collected if a GC is ongoing
        object->flags &= ~MARKBIT;
    }
    #ifdef PIK_DEBUG
    else {
        printf("%s at %p lost a reference (now have %zu)\n", type_name(object->type), (void*)object, object->refcnt);
    }
    #endif
}

static void mark_object(pickle_t vm, pik_object_t object) {
    mark:
    PIK_DEBUG_PRINTF("Marking %s at %p:\n", type_name(object->type), (void*)object);
    if (object == NULL || object->flags & MARKBIT) return;
    object->flags |= MARKBIT;
    // Mark payload
    PIK_DEBUG_PRINTF("%p->payload\n", (void*)object);
    switch (type_info(object->type) & CELL1_MASK) {
        case CELL1_EMPTY: break;
        case CELL1_CHARS: break;
        case CELL1_OBJECT: mark_object(vm, object->cell1); break;
    }
    switch (type_info(object->type) & CELL2_MASK) {
        case CELL2_EMPTY: break;
        case CELL2_FILE: break;
        case CELL2_OBJECT: mark_object(vm, object->cell2); break;
    }
    switch (type_info(object->type) & CELL3_MASK) {
        case CELL3_EMPTY: break;
        case CELL3_OBJECT: mark_object(vm, object->cell3); break;
    }
    // Mark properties
    PIK_DEBUG_PRINTF("%p->properties\n", (void*)object);
    mark_object(vm, object->properties);
    // Mark classes
    // Tail-call optimize
    object = object->classes;
    goto mark;
}

static void sweep_unmarked(pickle_t vm) {
    pik_object_t* object = &vm->first_object;
    while (*object != NULL) {
        PIK_DEBUG_PRINTF("Looking at %s at %p: flags=%#hx, ", type_name((*object)->type), (void*)(*object), (*object)->flags);
        if ((*object)->flags & MARKBIT) {
            PIK_DEBUG_PRINTF("marked\n");
            // Keep the object
            (*object)->flags &= ~MARKBIT;
            object = &(*object)->next_object;
        } else {
            PIK_DEBUG_PRINTF("unmarked\n");
            // Sweep the object
            pik_object_t unreached = *object;
            *object = unreached->next_object;
            finalize(vm, unreached);
            free(unreached);
            vm->num_objects--;
        }
    }
}

size_t pik_collect_garbage(pickle_t vm) {
    IF_NULL_RETURN(vm) 0;
    PIK_DEBUG_PRINTF("Collecting garbage\n");
    mark_object(vm, vm->global_scope);
    mark_object(vm, vm->dollar_function);
    mark_object(vm, vm->type_protos);
    size_t start = vm->num_objects;
    sweep_unmarked(vm);
    size_t freed = start - vm->num_objects;
    PIK_DEBUG_PRINTF("%zu freed, %zu objects remaining after gc\n", freed, vm->num_objects);
    return freed;
}

pickle_t pik_new(void) {
    pickle_t vm = (pickle_t)calloc(1, sizeof(struct pickle));
    PIK_DEBUG_PRINTF("For global scope: ");
    vm->global_scope = alloc_object(vm, SCOPE, 0);
    register_stdlib(vm);
    // TODO: register global functions
    return vm;
}

void pik_destroy(pickle_t vm) {
    IF_NULL_RETURN(vm);
    PIK_DEBUG_PRINTF("Freeing the VM - garbage collect all: ");
    vm->global_scope = NULL;
    vm->dollar_function = NULL;
    pik_collect_garbage(vm);
    PIK_DEBUG_ASSERT(vm->first_object == NULL, "Garbage collection failed to free all objects");
    PIK_DEBUG_PRINTF("Freeing VM\n");
    free(vm);
}

int pik_error(pickle_t vm, pik_object_t scope, const char* message) {
    pik_object_t e = alloc_object(vm, ERROR, 0);
    e->message = strdup(message);
    pik_decref(vm, scope->result);
    scope->result = e;
    pik_incref(e);
    return RERROR;
}

int pik_error_fmt(pickle_t vm, pik_object_t scope, const char* fmt, ...) {
    va_list args;
    va_start(args, fmt);
    char* message;
    vasprintf(&message, fmt, args);
    va_end(args);
    pik_error(vm, scope, message);
    free(message);
    return RERROR;
}

// Section: Parser

void pik_append(pik_object_t array, pik_object_t what) {
    array->items = (pik_object_t*)realloc(array->items, (array->len + 1) * sizeof(pik_object_t));
    array->items[array->len] = what;
    array->len++;
    pik_incref(what);
}

void pik_delete_at_index(pickle_t vm, pik_object_t array, size_t i) {
    pik_decref(vm, array->items[i]);
    if (i + 1 < array->len) {
        memmove((void*)&array->items[i], (void*)&array->items[i+1], (array->len - i - 1) * sizeof(pik_object_t));
    }
    array->items = (pik_object_t*)realloc(array->items, (array->len - 1) * sizeof(pik_object_t));
    array->len--;
    return;
}

#define PIK_DONE(vm, scope, rval) do { pik_decref(vm, scope->result); scope->result = rval; return ROK; } while (0)

typedef struct pik_parser {
    const char* code;
    size_t len;
    size_t head;
} pik_parser;

static inline char peek(pik_parser* p, size_t delta) {
    IF_NULL_RETURN(p) '\0';
    if ((p->head + delta) >= p->len) return '\0';
    return p->code[p->head + delta];
}

static inline char at(pik_parser* p) {
    return peek(p, 0);
}

static inline void advance(pik_parser* p, size_t delta) {
    IF_NULL_RETURN(p);
    if ((p->head + delta) <= p->len) p->head += delta;
}

static inline void next(pik_parser* p) {
    advance(p, 1);
}

static inline size_t save(pik_parser* p) {
    IF_NULL_RETURN(p) 0;
    return p->head;
}

static inline void restore(pik_parser* p, size_t i) {
    IF_NULL_RETURN(p);
    p->head = i;
}

static inline bool p_eof(pik_parser* p) {
    IF_NULL_RETURN(p) true;
    return p->head > p->len || at(p) == '\0';
}

static inline const char* str_of(pik_parser* p) {
    IF_NULL_RETURN(p) NULL;
    return &p->code[p->head];
}

static inline bool eolchar(char c) {
    return c == '\n' || c == '\r' || c == ';';
}

static inline bool p_endline(pik_parser* p) {
    IF_NULL_RETURN(p) true;
    if (p_eof(p)) return true;
    return eolchar(at(p));
}

static inline bool p_startswith(pik_parser* p, const char* str) {
    return strncmp(str_of(p), str, strlen(str)) == 0;
}

static inline char unescape(char c) {
    switch (c) {
        case 'b': return '\b';
        case 't': return '\t';
        case 'n': return '\n';
        case 'v': return '\v';
        case 'f': return '\f';
        case 'r': return '\r';
        case 'a': return '\a';
        case 'o': return '{';
        case 'c': return '}';
        default:  return c;
    }
}

static inline bool needs_escape(char c) {
    return strchr("{}\b\t\n\v\f\r\a\\\"", c) != NULL;
}

static inline char escape(char c) {
    switch (c) {
        case '\b': return 'b';
        case '\t': return 't';
        case '\n': return 'n';
        case '\v': return 'v';
        case '\f': return 'f';
        case '\r': return 'r';
        case '\a': return 'a';
        case '{': return 'o';
        case '}': return 'c';
        default:  return c;
    }
}

static bool valid_varchar(char c) {
    if ('A' <= c && 'Z' >= c) return true;
    if ('a' <= c && 'z' >= c) return true;
    if ('0' <= c && '9' >= c) return true;
    return strchr("#@?^_~", c) != NULL;
}

static bool valid_opchar(char c) {
    return strchr("`~!@#%^&*_-+=<>,./|:", c) != NULL;
}

static bool valid_wordchar(char c) {
    return strchr("[](){}\"';", c) == NULL;
}

static void skip_whitespace(pik_parser* p) {
    IF_NULL_RETURN(p);
    again:;
    size_t start = save(p);
    while (!p_eof(p)) {
        char c = at(p);
        if (c == '#') {
            if (p_startswith(p, "###")) {
                // Block comment
                advance(p, 3);
                while (!p_eof(p) && !p_startswith(p, "###")) next(p);
                advance(p, 3);
            } else {
                // Line comment
                while (!p_endline(p)) next(p);
            }
        } else if (c == '\\' && eolchar(peek(p, 1))) {
            // Escaped EOL
            next(p);
            while (!p_endline(p)) next(p);
        } else if (eolchar(c)) {
            // Not escaped EOL
            break;
        } else if (isspace(c)) {
            // Real space
            next(p);
        }
        else break;
    }
    // Try to get all the comments at once
    // if we got one, try for another
    if (p->head != start) {
        PIK_DEBUG_PRINTF("Skipped whitespace\n");
        goto again;
    }
    PIK_DEBUG_PRINTF("end charcode when done skipping whitespace: %u (%s%c)\n", at(p), needs_escape(at(p)) ? "\\" : "", escape(at(p)));
}

static int get_getvar(pickle_t vm, pik_parser* p, pik_object_t scope) {
    PIK_DEBUG_PRINTF("get_getvar()\n");
    next(p); // Skip $
    if (!valid_varchar(at(p))) {
        return pik_error_fmt(vm, scope, "syntax error: \"%s%c\" not allowed after \"$\"", needs_escape(at(p)) ? "\\" : "", escape(at(p)));
    }
    // First pass: find length
    size_t start = save(p);
    bool islambda = isdigit(at(p));
    while ((islambda ? isdigit(at(p)) : valid_varchar(at(p))) && !p_eof(p)) next(p);
    // Now pick it out
    size_t end = save(p);
    size_t len = end - start;
    restore(p, start);
    pik_object_t gv = alloc_object(vm, GETVAR, 0);
    asprintf(&gv->chars, "%.*s", (int)len, str_of(p));
    restore(p, end);
    PIK_DONE(vm, scope, gv);
}

static int get_string(pickle_t vm, pik_parser* p, pik_object_t scope) {
    char q = at(p);
    next(p);
    if (p_eof(p)) {
        char iq = q == '"' ? '\'' : '"';
        return pik_error_fmt(vm, scope, "syntax error: dangling %c%c%c", iq, q, iq);
    }
    PIK_DEBUG_PRINTF("get_string(%c)\n", q);
    // First pass: get length of string
    size_t start = save(p);
    size_t len = 0;
    while (at(p) != q) {
        len++;
        if (at(p) == '\\') advance(p, 2);
        else next(p);
        if (p_eof(p)) {
            restore(p, start - 1);
            return pik_error_fmt(vm, scope, "syntax error: unterminated string %.20s...", str_of(p));
        }
    }
    char* buf = (char*)calloc(len + 1, sizeof(char));
    // Second pass: grab the string
    restore(p, start);
    for (size_t i = 0; i < len; i++) {
        if (at(p) == '\\') {
            next(p);
            buf[i] = unescape(at(p));
        } else buf[i] = at(p);
        next(p);
    }
    next(p);
    pik_object_t stringobj = alloc_object(vm, STRING, 0);
    stringobj->chars = buf;
    PIK_DONE(vm, scope, stringobj);
}

static int get_brace_string(pickle_t vm, pik_parser* p, pik_object_t scope) {
    PIK_DEBUG_PRINTF("get_brace_string()\n");
    next(p); // Skip {
    if (p_eof(p)) {
        return pik_error(vm, scope, "syntax error: dangling \"{\"");
    }
    // First pass: find length
    size_t start = save(p);
    size_t depth = 1;
    while (true) {
        if (at(p) == '{') depth++;
        if (at(p) == '}') depth--;
        if (p_eof(p)) {
            restore(p, start - 1);
            return pik_error_fmt(vm, scope, "syntax error: unexpected EOF in curlies: %.20s...", str_of(p));
        }
        next(p);
        if (depth == 0) break;
    }
    // Now pick it out
    size_t end = save(p);
    restore(p, start);
    char* buf;
    size_t len = end - start - 1; // chop off } at end
    asprintf(&buf, "%.*s", (int)len, str_of(p));
    pik_object_t str = alloc_object(vm, STRING, 0);
    str->chars = buf;
    restore(p, end);
    PIK_DONE(vm, scope, str);
}

static int get_colon_string(pickle_t vm, pik_parser* p, pik_object_t scope) {
    PIK_DEBUG_PRINTF("get_colon_string()\n");
    while (at(p) != '\n') next(p); // Skip all before newline
    next(p); // Skip newline
    size_t indent = 0;
    bool spaces = at(p) == ' ';
    // Find first indent
    while (isspace(at(p))) {
        if (p_eof(p)) {
            return pik_error(vm, scope, "syntax error: unexpected EOF after \":\"");
        }
        if ((!spaces && at(p) == ' ') || (spaces && at(p) == '\t')) {
            return pik_error(vm, scope, "syntax error: mix of tabs and spaces indenting block");
        }
        indent++;
        next(p);
    }
    PIK_DEBUG_PRINTF("indent is %zu %s\n", indent, spaces ? "spaces" : "tabs");
    // Count size of string
    size_t start = save(p);
    size_t len = 0;
    while (true) {
        // Skip content of line
        while (at(p) != '\n') {
            next(p);
            if (p_eof(p)) goto stop;
            len++;
        }
        // +1 for \n
        size_t last_nl = save(p);
        next(p);
        len++;
        // Skip indent of line
        size_t this_indent = 0;
        while (isspace(at(p)) && this_indent < indent) {
            if ((!spaces && at(p) == ' ') || (spaces && at(p) == '\t')) {
                return pik_error(vm, scope, "syntax error: mix of tabs and spaces indenting block");
            }
            this_indent++;
            next(p);
            if (p_eof(p)) goto stop;
        }
        if (this_indent > 0 && this_indent < indent) {
            return pik_error(vm, scope, "syntax error: unindent does not match previous indent");
        }
        if (this_indent < indent) {
            // & at end means the next items are part of the same line
            if (at(p) != '&') {
                restore(p, last_nl);
            } else next(p);
            break;
        }
    }
    stop:;
    // Go back and get the string
    size_t end = save(p);
    restore(p, start);
    char* buf = (char*)calloc(len + 1, sizeof(char));
    for (size_t i = 0; i < len; i++) {
        buf[i] = at(p);
        if (at(p) == '\n') advance(p, indent); // Skip the indent
        next(p);
    }
    pik_object_t str = alloc_object(vm, STRING, 0);
    str->chars = buf;
    restore(p, end);
    PIK_DONE(vm, scope, str);
}

static int next_item(pickle_t vm, pik_parser* p, pik_object_t scope);

static int get_expression(pickle_t vm, pik_parser* p, pik_object_t scope) {
    PIK_DEBUG_PRINTF("get_expression()\n");
    next(p); // Skip (
    pik_object_t expr = alloc_object(vm, EXPRESSION, 0);
    while (true) {
        if (at(p) == ')') {
            next(p); // Skip )
            break;
        }
        int code = next_item(vm, p, scope);
        if (p_eof(p)) return pik_error(vm, scope, "unbalanced ()'s");
        if (code == RBREAK) {
            next(p);
            continue;
        }
        if (code == RERROR) return RERROR;
        if (scope->result) {
            pik_append(expr, scope->result);
        }
        #ifdef PIK_DEBUG
        else printf("Empty subexpr line\n");
        #endif
    }
    PIK_DONE(vm, scope, expr);
}

static int get_list(pickle_t vm, pik_parser* p, pik_object_t scope) {
    PIK_DEBUG_PRINTF("get_list()\n");
    next(p); // Skip [
    pik_object_t list = alloc_object(vm, LIST_LITERAL, 0);
    while (true) {
        if (at(p) == ']') {
            next(p); // Skip ]
            break;
        }
        int code = next_item(vm, p, scope);
        if (p_eof(p)) return pik_error(vm, scope, "unbalanced []'s");
        if (code == RBREAK) {
            next(p);
            continue;
        }
        if (code == RERROR) return RERROR;
        if (scope->result) {
            pik_append(list, scope->result);
        }
        #ifdef PIK_DEBUG
        else printf("Empty list line\n");
        #endif
    }
    PIK_DONE(vm, scope, list);
}

static int get_word(pickle_t vm, pik_parser* p, pik_object_t scope) {
    PIK_DEBUG_PRINTF("get_word()\n");
    size_t len = 0;
    // Special case: boolean
    if (p_startswith(p, "true") || p_startswith(p, "false")) {
        bool truthy = at(p) == 't';
        PIK_DEBUG_PRINTF("boolean %s\n", truthy ? "true" : "false");
        size_t start = save(p);
        advance(p, truthy ? 4 : 5);
        if (isspace(at(p)) || ispunct(at(p))) {
            pik_object_t result = alloc_object(vm, BOOLEAN, 0);
            result->boolean = truthy;
            PIK_DONE(vm, scope, result);
        }
        else restore(p, start);
    }
    else if (p_startswith(p, "nil") || p_startswith(p, "pass")) {
        bool pass = at(p) == 'p';
        PIK_DEBUG_PRINTF("nil\n");
        size_t start = save(p);
        advance(p, pass ? 4 : 3);
        if (isspace(at(p)) || ispunct(at(p))) {
            PIK_DONE(vm, scope, NULL);
        }
        else restore(p, start);
    }
    // Special case: numbers
    else if (isdigit(at(p))) {
        char c;
        // Complex
        float real; float imag;
        if (sscanf(str_of(p), "%g%gj%ln%c", &real, &imag, &len, &c) == 3) {
            advance(p, len);
            PIK_DEBUG_PRINTF("complex %g %+g * i\n", real, imag);
            pik_object_t result = alloc_object(vm, COMPLEX, 0);
            result->real = real;
            result->imag = imag;
            PIK_DONE(vm, scope, result);
        }
        // Rational
        int32_t num; uint32_t denom;
        if (sscanf(str_of(p), "%i/%u%ln%c", &num, &denom, &len, &c) == 3) {
            advance(p, len);
            PIK_DEBUG_PRINTF("rational %i over %u\n", num, denom);
            pik_object_t result = alloc_object(vm, RATIONAL, 0);
            result->numerator = num;
            result->denominator = denom;
            PIK_DONE(vm, scope, result);
        }
        // Integer
        int64_t intnum;
        if (sscanf(str_of(p), "%lli%ln%c", &intnum, &len, &c) == 2) {
            advance(p, len);
            PIK_DEBUG_PRINTF("integer %lli\n", intnum);
            pik_object_t result = alloc_object(vm, INTEGER, 0);
            result->integer = intnum;
            PIK_DONE(vm, scope, result);
        }
        // Float
        double floatnum;
        if (sscanf(str_of(p), "%lg%ln%c", &floatnum, &len, &c) == 2) {
            advance(p, len);
            PIK_DEBUG_PRINTF("float %lg\n", floatnum);
            pik_object_t result = alloc_object(vm, FLOAT, 0);
            result->floatnum = floatnum;
            PIK_DONE(vm, scope, result);
        }
    }
    // First pass: find length
    size_t start = save(p);
    bool is_operator = ispunct(at(p));
    while (!isspace(at(p)) && !p_eof(p) && !(valid_opchar(at(p)) ^ is_operator) && valid_wordchar(at(p))) {
        len++;
        next(p);
    }
    // Check if last is a colon, if it is a colon but there is non-whitespace before the newline
    // that means it is part of this word (get_colon_string() will ignore it otherwise)
    if (at(p) == ':') {
        size_t x = save(p);
        bool me_has_colon = true;
        next(p);
        while (isspace(at(p))) {
            if (at(p) == '\n') {
                me_has_colon = false;
                break;
            }
            next(p);
        }
        if (me_has_colon) {
            restore(p, x + 1);
            len++;
        } else restore(p, x);
    }
    // Pick it out
    size_t end = save(p);
    restore(p, start);
    pik_object_t w = alloc_object(vm, ispunct(at(p)) ? OPERATOR : SYMBOL, 0);
    asprintf(&w->chars, "%.*s", (int)len, str_of(p));
    restore(p, end);
    PIK_DONE(vm, scope, w);
}

static int next_item(pickle_t vm, pik_parser* p, pik_object_t scope) {
    IF_NULL_RETURN(vm) ROK;
    IF_NULL_RETURN(p) ROK;
    PIK_DEBUG_PRINTF("next_item()\n");
    if (p_eof(p)) return RBREAK;
    skip_whitespace(p);
    size_t here = save(p);
    int code = ROK;
    switch (at(p)) {
        case '$':  code = get_getvar(vm, p, scope); break;
        case '"':  // fallthrough
        case '\'': code = get_string(vm, p, scope); break;
        case '{':  code = get_brace_string(vm, p, scope); break;
        case '(':  code = get_expression(vm, p, scope); break;
        case '[':  code = get_list(vm, p, scope); break;
        case ']':  // fallthrough
        case ')':  return RBREAK;
        case '}':  pik_error(vm, scope, "syntax error: unexpected \"}\""); break;
        case ':':  if (strchr("\n\r", peek(p, 1))) { code = get_colon_string(vm, p, scope); break; } // else fallthrough
        default:   if (eolchar(at(p))) return RBREAK; else code = get_word(vm, p, scope); break;
    }
    if (code != RERROR && save(p) == here) {
        // Generic failed to parse message
        pik_error_fmt(vm, scope, "syntax error: failed to parse: %.20s...", str_of(p));
        code = RERROR;
    }
    return code;
}

int pik_compile(pickle_t vm, const char* code, pik_object_t scope) {
    pik_parser parser = { .code = code, .len = strlen(code) };
    pik_parser* p = &parser;
    IF_NULL_RETURN(vm) ROK;
    if (p_eof(p)) return ROK;
    PIK_DEBUG_PRINTF("Begin compile\n");
    pik_object_t block = alloc_object(vm, BLOCK, 0);
    pik_decref(vm, scope->result);
    scope->result = NULL;
    while (!p_eof(p)) {
        PIK_DEBUG_PRINTF("Beginning of line: ");
        pik_object_t line = alloc_object(vm, EXPRESSION, 0);
        while (!p_eof(p)) {
            PIK_DEBUG_PRINTF("Beginning of item: ");
            int result = next_item(vm, p, scope);
            if (result != RBREAK) pik_append(line, scope->result);
            else if (eolchar(at(p))) {
                next(p);
                break;
            } else {
                result = pik_error(vm, scope, "unknown parser error (too many close brackets?)");
            }
            if (result == RERROR) {
                pik_decref(vm, line);
                pik_decref(vm, block);
                return RERROR;
            }
        }
        if (line->len > 0) {
            if (line->items[0] && line->items[0]->type == SYMBOL) {
                line->items[0]->type = GETVAR;
            }
            pik_append(block, line);
            pik_decref(vm, line);
        }
        #ifdef PIK_DEBUG
        else printf("Empty line\n");
        #endif
    }
    PIK_DONE(vm, scope, block);
}

// Section: Evaluator

// Lispy equal? compare values
static bool equal(pik_object_t a, pik_object_t b) {
    compare:
    if (a == b) return true; // Same object
    if (a == NULL && b == NULL) return true; // Nil is the same as itself
    PIK_DEBUG_PRINTF("Comparing a %s and a %s\n", type_name(a->type), type_name(b->type));
    if (a->type != b->type) return false; // Not the same type so can't be equivalent
    switch (a->type) {
        case CONS:
            if (!equal(a->car, b->car)) return false;
            a = a->cdr;
            b = b->cdr;
            goto compare;
        case SYMBOL:
        case STRING:
        case ERROR:
            return streq(a->chars, b->chars);
        case INTEGER:
        case BOOLEAN:
        case FLOAT:
        case COMPLEX:
        case RATIONAL:
            return a->integer == b->integer;
        case BUILTIN_FUNCTION:
            return a->function == b->function && streq(a->chars, b->chars);
        case STREAM:
            return a->stream == b->stream && streq(a->chars, b->chars);
        case LIST:
        case MAP:
            goto compare_items;
        case KV_PAIR:
            if (!equal(a->key, b->key)) return false;
            a = a->value;
            b = b->value;
            goto compare;
        case CLASS:
            if (!equal(a->properties, b->properties)) return false;
            if (!equal(a->scope, b->scope)) return false;
            goto compare_items;
        case USER_FUNCTION:
            if (strcmp(a->name, b->name)) return false;
            if (!equal(a->scope, b->scope)) return false;
            a = a->properties;
            b = b->properties;
            goto compare;
        case ARGUMENT_ENTRY:
            if (strcmp(a->name, b->name)) return false;
            a = a->def;
            b = b->def;
            goto compare;
        case OPERATOR:
        case GETVAR:
            return streq(a->chars, b->chars);
        case EXPRESSION:
        case BLOCK:
        case LIST_LITERAL:
            goto compare_items;
        case SCOPE:
            if (!equal(a->properties, b->properties)) return false;
            if (!equal(a->result, b->result)) return false;
            goto compare_items;
        case BINDINGS_LIST:
            goto compare_items;
        case BINDING:
            if (strcmp(a->chars, b->chars)) return false;
            a = a->value;
            b = b->value;
            goto compare;
    }
    return true;
    compare_items:
    if (a->len != b->len) return false;
    for (size_t i = 0; i < a->len; i++) {
        if (!equal(a->items[i], b->items[i])) return false;
    }
    return true;
}

// Map stuff (for getting properties)
static void map_set(pickle_t vm, pik_object_t map, pik_object_t key, pik_object_t value) {
    IF_NULL_RETURN(vm);
    IF_NULL_RETURN(map);
    for (size_t i = 0; i < map->len; i++) {
        pik_object_t kvpair = map->items[i];
        if (equal(kvpair->key, key)) {
            pik_decref(vm, kvpair->value);
            pik_incref(value);
            kvpair->value = value;
            return;
        }
    }
    pik_object_t newpair = alloc_object(vm, KV_PAIR, 0);
    newpair->key = key;
    newpair->value = value;
    pik_incref(key);
    pik_incref(value);
    pik_append(map, newpair);
}

static pik_object_t map_get(pik_object_t map, pik_object_t key) {
    IF_NULL_RETURN(map) NULL;
    for (size_t i = 0; i < map->len; i++) {
        pik_object_t kvpair = map->items[i];
        if (equal(kvpair->key, key)) {
            pik_incref(kvpair->value);
            return kvpair->value;
        }
    }
    return NULL;
}

static bool map_has(pik_object_t map, pik_object_t key) {
    IF_NULL_RETURN(map) false;
    for (size_t i = 0; i < map->len; i++) {
        if (equal(map->items[i]->key, key)) return true;
    }
    return false;
}

static void map_delete(pickle_t vm, pik_object_t map, pik_object_t key) {
    IF_NULL_RETURN(map);
    for (size_t i = 0; i < map->len; i++) {
        pik_object_t kvpair = map->items[i];
        if (equal(kvpair->key, key)) {
            pik_delete_at_index(vm, map, i);
            return;
        }
    }
}

// Scope set, get, has, delete
void scope_set(pickle_t vm, pik_object_t scope, const char* name, pik_object_t value) {
    IF_NULL_RETURN(vm);
    IF_NULL_RETURN(scope);
    pik_object_t b = scope->properties;
    if (b == NULL) {
        b = alloc_object(vm, BINDINGS_LIST, 0);
        b->len = 0;
        scope->properties = b;
    }
    for (size_t i = 0; i < b->len; i++) {
        pik_object_t entry = b->items[i];
        if (streq(entry->chars, name)) {
            pik_decref(vm, entry->value);
            pik_incref(value);
            entry->value = value;
            return;
        }
    }
    pik_object_t newentry = alloc_object(vm, BINDING, 0);
    newentry->chars = strdup(name);
    newentry->value = value;
    pik_incref(value);
    pik_append(b, newentry);
}

static pik_object_t scope_get(pik_object_t scope, const char* name) {
    top:
    IF_NULL_RETURN(scope) NULL;
    pik_object_t b = scope->properties;
    if (!b) goto bottom;
    for (size_t i = 0; i < b->len; i++) {
        if (streq(b->items[i]->chars, name)) {
            pik_incref(b->value);
            return b->value;
        }
    }
    bottom:
    if (!scope->classes || !scope->classes->len) return NULL;
    scope = scope->classes->items[0];
    goto top;
}

static bool scope_has(pik_object_t scope, const char* name) {
    top:
    IF_NULL_RETURN(scope) false;
    pik_object_t b = scope->properties;
    if (b == NULL) {
        if (!scope->classes || !scope->classes->len) return NULL;
        scope = scope->classes->items[0];
        goto top;
    }
    for (size_t i = 0; i < b->len; i++) {
        if (streq(b->items[i]->chars, name)) return true;
    }
    if (!scope->classes || !scope->classes->len) return NULL;
    scope = scope->classes->items[0];
    goto top;
}

static void scope_delete(pickle_t vm, pik_object_t scope, pik_object_t key) {
    pik_error(vm, scope, "scope_delete() unimplemented");
}

// Get/set variable wrappers to abstract the dollar_function
static int get_var(pickle_t vm, const char* name, pik_object_t args, pik_object_t scope) {
    IF_NULL_RETURN(vm) ROK;
    // TODO: special vars, index-based, last result, etc.
    pik_object_t dollar = vm->dollar_function;
    pik_object_t symbol = alloc_object(vm, SYMBOL, 0);
    symbol->chars = strdup(name);
    pik_object_t alist = alloc_object(vm, LIST, 0);
    pik_append(alist, symbol);
    pik_decref(vm, symbol);
    return call(vm, NULL, dollar, alist, scope);
}

static int set_var(pickle_t vm, const char* name, pik_object_t value, pik_object_t scope) {
    IF_NULL_RETURN(vm) ROK;
    // TODO: bail if special var, index-based, etc.
    pik_object_t dollar = vm->dollar_function;
    pik_object_t symbol = alloc_object(vm, SYMBOL, 0);
    symbol->chars = strdup(name);
    pik_object_t alist = alloc_object(vm, LIST, 0);
    pik_append(alist, symbol);
    pik_append(alist, value);
    pik_decref(vm, symbol);
    pik_decref(vm, value);
    return call(vm, NULL, dollar, alist, scope);
}

// Get/set properties on an object
static int get_property(pickle_t vm, pik_object_t object, pik_object_t scope, const char* property, bool try_getprop) {
    IF_NULL_RETURN(vm) ROK;
    PIK_DEBUG_PRINTF("Get_property %s on object %p\n", property, (void*) object);
    pik_object_t foo = alloc_object(vm, SYMBOL, 0);
    foo->chars = strdup(property);
    // try object itself
    if (object->properties) {
        if (map_has(object->properties, foo)) {
            pik_object_t bar = map_get(object->properties, foo);
            PIK_DONE(vm, scope, bar);
        }
    }
    // check prototypes
    if (object->classes) {
        for (size_t i = 0; i < object->classes->len; i++) {
            if (get_property(vm, object->classes->items[i], scope, property, true) == ROK) return ROK;
            // if OK, result was set on scope->result already
        }
    }
    // call __getprop__
    if (try_getprop) {
        if (get_property(vm, object, scope, "__getprop__", false) == RERROR) return RERROR;
        pik_object_t args = alloc_object(vm, LIST, 0);
        pik_append(args, foo);
        pik_decref(vm, foo);
        return call(vm, scope->result, object, args, scope);
    } else {
        return pik_error_fmt(vm, scope, "object has no property %s", property);
    }
}

static int set_property(pickle_t vm, pik_object_t object, pik_object_t scope, const char* property, pik_object_t value) {
    return pik_error(vm, scope, "unimplemented set_property()");
}

// Call an expression with arguments in a scope
static int call(pickle_t vm, pik_object_t func, pik_object_t self, pik_object_t args, pik_object_t scope) {
    IF_NULL_RETURN(vm) ROK;
    try_again:
    PIK_DEBUG_PRINTF("call(%s %s %u)\n", func ? type_name(func->type) : "NULL", self ? type_name(self->type) : "NULL", args ? args->len : 0);
    if (func == NULL && self == NULL && args != NULL && args->len > 0) return pik_error(vm, scope, "can't call NULL");
    if (scope == NULL) scope = vm->global_scope;
    if (func == NULL) {
        PIK_DEBUG_PRINTF("trying to call self\n");
        int ret = call(vm, self, NULL, args, scope);
        if (ret != RERROR) return ret;
        PIK_DEBUG_PRINTF("NO function\n");
        if (!args->len) {
            PIK_DEBUG_PRINTF("No args, return the object\n");
            PIK_DONE(vm, scope, self);
        }
        PIK_DEBUG_PRINTF("Has args, trying to call self\n");
        // try to __call__ the object; create a temporory bound method
        pik_object_t bound = alloc_object(vm, BOUND_METHOD, 0);
        bound->bound_self = self;
        if (get_property(vm, self, scope, "__call__", true) == RERROR) return pik_error_fmt(vm, scope, "can't call %s", type_name(self->type));
        bound->bound_method = scope->result;
        func = bound;
        goto try_again;
    }
    else if (func->type == BUILTIN_FUNCTION) return func->function(vm, self, args, scope);
    else if (func->type == BOUND_METHOD) {
        PIK_DEBUG_PRINTF("Got bound method, unpacking\n");
        self = func->bound_self;
        pik_object_t tempfunc = func->bound_method;
        pik_incref(self);
        pik_incref(tempfunc);
        pik_decref(vm, func);
        func = tempfunc;
        goto try_again;
    }
    else if (func->type == USER_FUNCTION) {
        pik_object_t newscope = alloc_object(vm, SCOPE, 0);
        newscope->classes = alloc_object(vm, LIST, 0);
        pik_append(newscope->classes, func->scope);
        pik_incref(func->scope);
        // Set parameters
        size_t argn = 0;
        pik_object_t argnames = func->properties;
        for (size_t i = 0; i < argnames->len; i++) {
            if (set_var(vm, argnames->items[i]->name, i >= args->len ? argnames->items[i]->def : args->items[argn], newscope) == RERROR) return RERROR;
        }
        // if there are too many arguments they are silently ignored but can be accessed through $@.
        // now we have arg->chars == NULL, ->rest == body
        // eval the body progn style in a block
        if (eval_block(vm, self, func->body, args, newscope) == RERROR) return RERROR;
        pik_decref(vm, scope->result);
        scope->result = newscope->result;
        pik_incref(newscope->result);
        pik_decref(vm, newscope);
        return ROK;
    }
    else {
        PIK_DEBUG_PRINTF("Func is not callable trying to call self\n");
        self = func;
        func = NULL;
        goto try_again;
    }
}

// Eval remainder of expression (items 1...end); return new expression
static int eval_remainder(pickle_t vm, pik_object_t self, pik_object_t line, pik_object_t args, pik_object_t scope) {
    IF_NULL_RETURN(vm) ROK;
    IF_NULL_RETURN(line) ROK;
    PIK_DEBUG_PRINTF("eval_remainder()\n");
    if (line->len < 2) {
        PIK_DONE(vm, scope, line);
    }
    pik_object_t newexpr = alloc_object(vm, EXPRESSION, 0);
    pik_append(newexpr, line->items[0]);
    for (size_t i = 1; i < line->len; i++) {
        int code = pik_eval(vm, self, line->items[i], args, scope);
        if (code != ROK) return code;
        pik_append(newexpr, scope->result);
    }
    PIK_DONE(vm, scope, newexpr);
}

static int eval_getvar(pickle_t vm, pik_object_t self, pik_object_t getvar, pik_object_t args, pik_object_t scope) {
    PIK_DEBUG_PRINTF("eval_getvar()\n");
    return get_var(vm, getvar->chars, args, scope);
}

// Eval different AST nodes
// Eval block using progn (no TCO yet)
static int eval_block(pickle_t vm, pik_object_t self, pik_object_t block, pik_object_t args, pik_object_t scope) {
    IF_NULL_RETURN(vm) ROK;
    IF_NULL_RETURN(block) ROK;
    PIK_DEBUG_PRINTF("eval_block(%u)\n", block->len);
    if (block->len == 0) PIK_DONE(vm, scope, NULL);
    for (size_t i = 0; i < block->len; i++) {
        int code = pik_eval(vm, self, block->items[i], args, scope);
        PIK_DEBUG_PRINTF("block eval code %i\n", code);
        if (code != ROK) return code;
        scope_set(vm, scope, "_", scope->result);
    }
    return ROK;
}

static int reduce_expression(pickle_t vm, pik_object_t self, pik_object_t expr, pik_object_t scope) {
    PIK_DEBUG_PRINTF("reduce_expression(%u)\n", expr->len);
    if (expr->len == 0) PIK_DONE(vm, scope, expr);
    // Call eval_remainder
    return pik_error(vm, scope, "reduce_expression unimplemented");
}

static bool is_macro(pik_object_t func) {
    top:
    IF_NULL_RETURN(func) false;
    switch (func->type) {
        case USER_FUNCTION:
        case BUILTIN_FUNCTION:
            return func->flags & FUNCTION_IS_MACRO;
        case BOUND_METHOD:
            func = func->bound_method;
            goto top;
        default:
            return false;
    }
}

static int eval_expression(pickle_t vm, pik_object_t self, pik_object_t expr, pik_object_t args, pik_object_t scope) {
    IF_NULL_RETURN(vm) ROK;
    IF_NULL_RETURN(expr) ROK;
    PIK_DEBUG_PRINTF("eval_expression(%u)\n", expr->len);
    if (scope == NULL) scope = vm->global_scope;
    if (expr->len == 0) PIK_DONE(vm, scope, NULL);
    if (pik_eval(vm, self, expr->items[0], args, scope) == RERROR) return RERROR;
    pik_object_t call_args = alloc_object(vm, LIST, 0);
    pik_object_t func = scope->result;
    pik_incref(func);
    if (is_macro(func)) {
        PIK_DEBUG_PRINTF("is macro\n");
        for (size_t i = 0; i < expr->len - 1; i++) {
            pik_append(call_args, expr->items[i+1]);
        }
    } else {
        PIK_DEBUG_PRINTF("not macro\n");
        if (reduce_expression(vm, self, expr, scope) == RERROR) return RERROR;
        pik_object_t reduced = scope->result;
        for (size_t i = 1; i < reduced->len - 1; i++) {
            pik_append(call_args, reduced->items[i+1]);
        }
        pik_decref(vm, func);
        func = reduced->items[0];
        pik_incref(func);
    }
    return call(vm, func, self, call_args, scope);
}

static int eval_to_list(pickle_t vm, pik_object_t self, pik_object_t list, pik_object_t args, pik_object_t scope) {
    IF_NULL_RETURN(vm) ROK;
    IF_NULL_RETURN(list) ROK;
    PIK_DEBUG_PRINTF("eval_to_list()\n");
    if (scope == NULL) scope = vm->global_scope;
    pik_decref(vm, scope->result);
    scope->result = NULL;
    pik_object_t newlist = alloc_object(vm, LIST, 0);
    for (size_t i = 0; i < list->len; i++) {
        if (pik_eval(vm, self, list->items[i], args, scope) == RERROR) return RERROR;
        pik_append(newlist, scope->result);
    }
    PIK_DONE(vm, scope, newlist);
}

int pik_eval(pickle_t vm, pik_object_t self, pik_object_t x, pik_object_t args, pik_object_t scope) {
    IF_NULL_RETURN(vm) ROK;
    IF_NULL_RETURN(x) ROK;
    PIK_DEBUG_PRINTF("evaluating %s at %p\n", type_name(x->type), (void*)x);
    if (scope == NULL) scope = vm->global_scope;
    switch (x->type) {
        case GETVAR: return eval_getvar(vm, self, x, args, scope);
        case EXPRESSION: return eval_expression(vm, self, x, args, scope);
        case BLOCK: return eval_block(vm, self, x, args, scope);
        case LIST_LITERAL: return eval_to_list(vm, self, x, args, scope);
        default: PIK_DONE(vm, scope, x);
    }
}

// Section: Printer

static void dump_ast(pik_object_t code, int indent, FILE* s) {
    if (code == NULL) {
        fprintf(s, "NULL");
        return;
    }
    switch (code->type) {
        case CONS:
            fprintf(s, "cons(\n");
            fprintf(s, "%*scar: ", (indent + 1) * 4, "");
            dump_ast(code->car, indent + 1, s);
            fprintf(s, ",\n%*scdr: ", (indent + 1) * 4, "");
            dump_ast(code->cdr, indent + 1, s);
            fprintf(s, "\n%*s)", indent * 4, "");
            break;
        case SYMBOL:
            fprintf(s, "symbol(%s)", code->chars);
            break;
        case STRING:
            fprintf(s, "string(\"");
            for (size_t i = 0; i < strlen(code->chars); i++) {
                if (needs_escape(code->chars[i])) fputchar(s, '\\');
                fputchar(s, escape(code->chars[i]));
            }
            fprintf(s, "\")");
            break;
        case ERROR:
            fprintf(s, "%serror(%s)", code->flags & ERROR_HAS_BEEN_CAUGHT ? "caught_" : "", code->message);
            break;
        case INTEGER:
            fprintf(s, "int(%lli)", code->integer);
            break;
        case BOOLEAN:
            fprintf(s, "bool(%s)", code->boolean ? "true" : "false");
            break;
        case FLOAT:
            fprintf(s, "float(%lg)", code->floatnum);
            break;
        case COMPLEX:
            fprintf(s, "complex(%g%+gj)", code->real, code->imag);
            break;
        case RATIONAL:
            fprintf(s, "rational(%i/%u)", code->numerator, code->denominator);
            break;
        case BUILTIN_FUNCTION:
            fprintf(s, "builtin_function(%s at %p)", code->chars, (void*)code->function);
            break;
        case STREAM:
            fprintf(s, "stream(%s at byte %zu)", code->chars, ftell(code->stream));
            break;
        case LIST:
            fprintf(s, "list(\n");
            goto dump_items;
        case MAP:
            fprintf(s, "map(\n");
            goto dump_items;
        case KV_PAIR:
            fprintf(s, "kv_pair(\n");
            fprintf(s, "%*skey: ", (indent + 1) * 4, "");
            dump_ast(code->key, indent + 1, s);
            fprintf(s, ",\n%*sval: ", (indent + 1) * 4, "");
            dump_ast(code->value, indent + 1, s);
            fprintf(s, "\n%*s)", indent * 4, "");
            break;
        case CLASS:
            fprintf(s, "class(");
            fprintf(s, "%*snamespace: ", (indent + 1) * 4, "");
            dump_ast(code->properties, indent + 1, s);
            fprintf(s, ",\n%*sscope: ", (indent + 1) * 4, "");
            dump_ast(code->scope, indent + 1, s);
            fprintf(s, ",\n%*sparents: ", (indent + 1) * 4, "");
            goto dump_items;
        case USER_FUNCTION:
            fprintf(s, "user_function(");
            fprintf(s, "%*sname: %s,", (indent + 1) * 4, "", code->name);
            fprintf(s, "\n%*sscope: ", (indent + 1) * 4, "");
            dump_ast(code->scope, indent + 1, s);
            fprintf(s, ",\n%*sbody: ", (indent + 1) * 4, "");
            dump_ast(code->body, indent + 1, s);
            fprintf(s, "\n%*s)", indent * 4, "");
            break;
        case ARGUMENT_ENTRY:
            fprintf(s, "arg_entry(");
            fprintf(s, "%*sname: %s,", (indent + 1) * 4, "", code->name);
            fprintf(s, "\n%*sdefault: ", (indent + 1) * 4, "");
            dump_ast(code->def, indent + 1, s);
            fprintf(s, "\n%*s)", indent * 4, "");
            break;
        case OPERATOR:
            fprintf(s, "operator(%s)", code->chars);
            break;
        case GETVAR:
            fprintf(s, "getvar(%s)", code->chars);
            break;
        case EXPRESSION:
            fprintf(s, "expr(\n");
            goto dump_items;
        case BLOCK:
            fprintf(s, "block(\n");
            goto dump_items;
        case LIST_LITERAL:
            fprintf(s, "list_literal(\n");
            goto dump_items;
        case SCOPE:
            fprintf(s, "scope(");
            fprintf(s, "%*sresult: ", (indent + 1) * 4, "");
            dump_ast(code->result, indent + 1, s);
            fprintf(s, ",\n%*sbindings: ", (indent + 1) * 4, "");
            dump_ast(code->properties, indent + 1, s);
            fprintf(s, ",\n%*soperators: ", (indent + 1) * 4, "");
            goto dump_items;
        case BINDINGS_LIST:
            fprintf(s, "bindings_list(\n");
            goto dump_items;
        case BINDING:
            fprintf(s, "binding(%s -> ", code->chars);
            dump_ast(code->value, indent + 1, s);
            fprintf(s, ")");
            break;
        default:
            fprintf(s, "<object type %u at %p>", code->type, (void*)code);
    }
    return;
    dump_items:
    for (size_t i = 0; i < code->len; i++) {
        if (i) fprintf(s, ",\n");
        fprintf(s, "%*s", (indent + 1) * 4, "");
        dump_ast(code->items[i], indent + 1, s);
    }
    fprintf(s, "\n%*s)", indent * 4, "");
}

void pik_print_to(pickle_t vm, pik_object_t object, FILE* s) {
    IF_NULL_RETURN(object);
    dump_ast(object, 0, s);
}

// Section: Builtin functions

static int getvar_func(pickle_t vm, pik_object_t self, pik_object_t args, pik_object_t scope) {
    PIK_DONE(vm, scope, NULL);
}

static void register_stdlib(pickle_t vm) {
    PIK_DEBUG_PRINTF("register standard library\n");
    pik_object_t var = alloc_object(vm, BUILTIN_FUNCTION, 0);
    var->chars = strdup("let");
    var->function = getvar_func;
    vm->dollar_function = var;
}


// Section: REPL

#ifdef PIK_TEST
void repl(pickle_t vm) {
    char* buf = (char*)malloc(64 * sizeof(char));
    char* codebuf = (char*)malloc(64 * sizeof(char));
    size_t sz = 64;
    while (true) {
        printf("pickle> ");
        fflush(stdout);
        codebuf[0] = 0;
        while (true) {
            if (getline(&buf, &sz, stdin) == -1) {
                printf("^D\n");
                goto done;
            }
            if (strlen(buf) == 1) break; // getline includes the terminating newline
            printf("   ...> ");
            fflush(stdout);
            char* newcode;
            asprintf(&newcode, "%s%s", codebuf, buf);
            free(codebuf);
            codebuf = newcode;
        }
        if (!strncmp(codebuf, "bye", 3)) {
            goto done;
        }
        pik_collect_garbage(vm);
        if (pik_compile(vm, codebuf, vm->global_scope) == RERROR) {
            printf("Compile error!\n%s\n", vm->global_scope->result->message);
            continue;
        }
        printf("executing:\n");
        pik_print_to(vm, vm->global_scope->result, stdout);
        putchar('\n');
        if (pik_eval(vm, NULL, vm->global_scope->result, NULL, vm->global_scope) == RERROR) {
            printf("Execution error!\n%s\n", vm->global_scope->result->message);
            continue;
        }
        printf("result> ");
        pik_print_to(vm, vm->global_scope->result, stdout);
        putchar('\n');
    }
    done:
    free(buf);
    free(codebuf);
    return;
}

int main() {
    printf("sizes: pik_object_t=%zu, pickle_t=%zu\n", sizeof(struct pik_object), sizeof(struct pickle));
    pickle_t vm = pik_new();
    repl(vm);
    pik_destroy(vm);
    return 0;
}
#endif

#ifdef __cplusplus
}
#endif
