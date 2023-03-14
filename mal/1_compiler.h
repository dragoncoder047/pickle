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
    CLASS,              // parents     | scope     | namespace
    USER_FUNCTION,      // name        | scope     | arguments

    // Internal types
    ARGUMENT_ENTRY,     // name        | default   | rest
    OPERATOR,           // char*       | (empty)   | (empty)
    GETVAR,             // char*       | (empty)   | (empty)
    EXPRESSION,         // item**s     | len       | (empty)
    BLOCK,              // item**s     | len       | (empty)
    LIST_LITERAL,       // item**s     | len       | (empty)
    SCOPE,              // bindings    | result    | parent   -- code is in subtype
    BINDINGS_LIST,      // item**s     | len       | (empty)
    BINDING             // char*       | value     | (empty)
};

enum flag {
    MARKBIT = 1,
    FINALIZED = 2,
    ERROR_HAS_BEEN_CAUGHT = 4,
    FUNCTION_IS_TCO = 4
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
        uint32_t col;
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
                pik_object_t cell1, car, item, key, parents, name, bindings, symbol, element;
                pik_object_t* items;
                char* chars;
                char* message;
                float real;
                int32_t numerator;
                uint8_t boolean;
            };
            // Cell 2
            union {
                pik_object_t cell2, cdr, value, scope, def, result, args;
                size_t len;
                float imag;
                uint32_t denominator;
                FILE* stream;
                pik_func function;
            };
            // Cell 3
            union {
                pik_object_t cell3, rest, ns, arguments, parent;
            };
        };
    };
};


typedef struct pik_operator {
    int precedence;
    char* symbol;
    char* method;
} pik_operator;

struct pickle {
    pik_object_t first_object;
    size_t num_objects;
    pik_operator* operators;
    size_t num_operators;
    pik_object_t global_scope;
    pik_object_t dollar_function;
};

// Section: Forward references
void pik_decref(pickle_t vm, pik_object_t object);
static void register_stdlib(pickle_t vm);

// Section: Garbage Collector

// #define PIK_DOCALLBACK(vm, name, ...) \
// if (vm->callbacks.name) { \
//     vm->callbacks.name(vm, __VA_ARGS__);\
// }

static pik_object_t alloc_object(pickle_t vm, uint16_t type, uint16_t subtype) {
    pik_object* object = vm->first_object;
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
    PIK_DEBUG_PRINTF("at %p\n", (void*)object);
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
    PIK_DEBUG_PRINTF("object %p got a new reference (now have %zu)\n", (void*)object, object->refcnt);
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
        case CLASS:             return CELL1_OBJECT  | CELL2_OBJECT | CELL3_OBJECT;
        case USER_FUNCTION:     return CELL1_OBJECT  | CELL2_OBJECT | CELL3_OBJECT;
        case ARGUMENT_ENTRY:    return CELL1_OBJECT  | CELL2_OBJECT | CELL3_EMPTY;
        case GETVAR:            return CELL1_CHARS   | CELL2_EMPTY  | CELL3_EMPTY;
        case OPERATOR:          return CELL1_CHARS   | CELL2_EMPTY  | CELL3_EMPTY;
        case EXPRESSION:        return CELL1_OBJECTS | CELL2_EMPTY  | CELL3_EMPTY;
        case BLOCK:             return CELL1_OBJECTS | CELL2_EMPTY  | CELL3_EMPTY;
        case LIST_LITERAL:      return CELL1_OBJECTS | CELL2_EMPTY  | CELL3_EMPTY;
        case SCOPE:             return CELL1_OBJECT  | CELL2_OBJECT | CELL3_OBJECT;
        case BINDINGS_LIST:     return CELL1_OBJECTS | CELL2_EMPTY  | CELL3_EMPTY;
        case BINDING:           return CELL1_CHARS   | CELL2_OBJECT | CELL3_EMPTY;
    }
    return 0; // Satisfy compiler
}

static void finalize(pickle_t vm, pik_object_t object) {
    IF_NULL_RETURN(object);
    if (object->flags & FINALIZED) {
        PIK_DEBUG_PRINTF("Already finalized object at %p\n", (void*)object);
        return;
    }
    PIK_DEBUG_PRINTF("Finalizing object at %p\n", (void*)object);
    // Free object-specific stuff
    switch (type_info(object->type) & CELL1_MASK) {
        case CELL1_EMPTY: break;
        case CELL1_CHARS: free(object->chars); object->chars = NULL; break;
        case CELL1_OBJECT: pik_decref(vm, object->cell1); break;
    }
    switch (type_info(object->type) & CELL2_MASK) {
        case CELL2_EMPTY: break;
        case CELL2_FILE: fclose(object->stream); object->stream = NULL; break;
        case CELL2_OBJECT: pik_decref(vm, object->cell2); break;
    }
    switch (type_info(object->type) & CELL3_MASK) {
        case CELL3_EMPTY: break;
        case CELL3_OBJECT: pik_decref(vm, object->cell3); break;
    }
    // Free everything else
    object->flags = FINALIZED;
    pik_decref(vm, object->classes);
    pik_decref(vm, object->properties);
    object->properties = object->classes = NULL;
}

void pik_decref(pickle_t vm, pik_object_t object) {
    IF_NULL_RETURN(object);
    PIK_DEBUG_ASSERT(object->refcnt > 0, "Decref'ed an object with 0 references already");
    object->refcnt--;
    if (object->refcnt == 0) {
        PIK_DEBUG_PRINTF("object at %p lost all references, finalizing\n", (void*)object);
        // Free it now, no other references
        finalize(vm, object);
        // Unmark it so it will be collected if a GC is ongoing
        object->flags &= ~MARKBIT;
    }
    #ifdef PIK_DEBUG
    else {
        printf("object at %p lost a reference (now have %zu)\n", (void*)object, object->refcnt);
    }
    #endif
}

static void mark_object(pickle_t vm, pik_object_t object) {
    mark:
    PIK_DEBUG_PRINTF("Marking object at %p:\n", (void*)object);
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
    pik_object** object = &vm->first_object;
    while (*object != NULL) {
        PIK_DEBUG_PRINTF("Looking at object at %p: flags=%#hx, ", (void*)(*object), (*object)->flags);
        if ((*object)->flags & MARKBIT) {
            PIK_DEBUG_PRINTF("marked\n");
            // Keep the object
            (*object)->flags &= ~MARKBIT;
            object = &(*object)->next_object;
        } else {
            PIK_DEBUG_PRINTF("unmarked\n");
            // Sweep the object
            pik_object* unreached = *object;
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
    PIK_DEBUG_PRINTF("Freeing %zu operators\n", vm->num_operators);
    for (size_t i = 0; i < vm->num_operators; i++) {
        PIK_DEBUG_PRINTF(" -- %s __%s__\n", vm->operators[i].symbol, vm->operators[i].method);
        free(vm->operators[i].symbol);
        free(vm->operators[i].method);
    }
    free(vm->operators);
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
    return strchr("#@?^.~", c) != NULL;
}

static bool valid_opchar(char c) {
    return strchr("`~!@#%^&*_-+=<>,./|:;", c) != NULL;
}

static bool valid_wordchar(char c) {
    return strchr("[](){}\"'", c) == NULL;
}

static bool skip_whitespace(pik_parser* p) {
    IF_NULL_RETURN(p) true;
    bool skipped = false;
    again:;
    size_t start = save(p);
    while (!p_eof(p)) {
        char c = at(p);
        if (c == '#') {
            if (p_startswith(p, "###")) {
                // Block comment
                advance(p, 2);
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
        skipped = true;
        PIK_DEBUG_PRINTF("Skipped whitespace\n");
        goto again;
    }
    PIK_DEBUG_PRINTF("end charcode when done skipping whitespace: %u (%s%c)\n", at(p), needs_escape(at(p)) ? "\\" : "", escape(at(p)));
    return skipped;
}

static int get_getvar(pickle_t vm, pik_parser* p, pik_object_t scope) {
    PIK_DEBUG_PRINTF("get_getvar()\n");
    next(p); // Skip $
    if (!valid_varchar(at(p))) {
        return pik_error_fmt(vm, scope, "syntax error: \"%s%c\" not allowed after \"$\"", needs_escape(at(p)) ? "\\" : "", escape(at(p)));
    }
    // First pass: find length
    size_t len = 0;
    size_t start = save(p);
    bool islambda = isdigit(at(p));
    while ((islambda ? isdigit(at(p)) : valid_varchar(at(p))) && !p_eof(p)) {
        len++;
        next(p);
    }
    // Now pick it out
    size_t end = save(p);
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
    size_t len = 0;
    size_t start = save(p);
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
    size_t len = 0;
    size_t start = save(p);
    size_t depth = 1;
    while (true) {
        if (at(p) == '{') depth++;
        if (at(p) == '}') depth--;
        if (p_eof(p)) {
            restore(p, start - 1);
            return pik_error_fmt(vm, scope, "syntax error: unbalanced curlies: %.20s...", str_of(p));
        }
        next(p);
        if (depth == 0) break;
        len++;
    }
    // Now pick it out
    size_t end = save(p);
    restore(p, start);
    char* buf;
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
        if (next_item(vm, p, scope) == RERROR) return RERROR;
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
        if (next_item(vm, p, scope) == RERROR) return RERROR;
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
        int64_t truthy = at(p) == 't';
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
    // Special case: numbers
    if (isdigit(at(p))) {
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
    IF_NULL_RETURN(vm) NULL;
    IF_NULL_RETURN(p) NULL;
    PIK_DEBUG_PRINTF("next_item()\n");
    if (p_eof(p)) return ROK;
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
        case ')':  PIK_DONE(vm, scope, NULL); // allow get_expression() and get_list() to see their end
        case '}':  pik_error(vm, scope, "syntax error: unexpected \"}\""); break;
        case ':':  if (strchr("\n\r", peek(p, 1))) { code = get_colon_string(vm, p, scope); break; } // else fallthrough
        default:   if (eolchar(at(p))) return ROK; else code = get_word(vm, p, scope); break;
    }
    if (code != RERROR && save(p) == here) {
        // Generic failed to parse message
        pik_error_fmt(vm, scope, "syntax error: failed to parse: %.20s...", str_of(p));
        code = RERROR;
    }
    if (code == RERROR) {
        return RERROR;
    }
    return ROK;
}

int pik_compile(pickle_t vm, const char* code, pik_object_t scope) {
    pik_parser parser = { .code = code, .len = strlen(code) };
    pik_parser* p = &parser;
    IF_NULL_RETURN(vm) ROK;
    if (p_eof(p)) return ROK;
    PIK_DEBUG_PRINTF("Begin compile\n");
    pik_object_t block = alloc_object(vm, BLOCK, 0);
    while (!p_eof(p)) {
        PIK_DEBUG_PRINTF("Beginning of line: ");
        pik_object_t line = alloc_object(vm, EXPRESSION, 0);
        while (!p_eof(p)) {
            PIK_DEBUG_PRINTF("Beginning of item: ");
            if (next_item(vm, p, scope) == RERROR) {
                pik_decref(vm, line);
                pik_decref(vm, block);
                return RERROR;
            }
            if (scope->result) pik_append(line, scope->result);
            if (eolchar(at(p))) {
                next(p);
                break;
            }
        }
        if (line->len > 0) {
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

int pik_eval(pickle_t vm, pik_object_t x, pik_object_t scope) {
    pik_incref(x);
    PIK_DONE(vm, scope, x);
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
            fprintf(s, "%*sparents: ", (indent + 1) * 4, "");
            dump_ast(code->parents, indent + 1, s);
            fprintf(s, ",\n%*sscope: ", (indent + 1) * 4, "");
            dump_ast(code->scope, indent + 1, s);
            fprintf(s, ",\n%*ss: ", (indent + 1) * 4, "");
            dump_ast(code->ns, indent + 1, s);
            fprintf(s, "\n%*s)", indent * 4, "");
            break;
        case USER_FUNCTION:
            fprintf(s, "class(");
            fprintf(s, "%*sname: ", (indent + 1) * 4, "");
            dump_ast(code->name, indent + 1, s);
            fprintf(s, ",\n%*sscope: ", (indent + 1) * 4, "");
            dump_ast(code->scope, indent + 1, s);
            fprintf(s, ",\n%*sargs: ", (indent + 1) * 4, "");
            dump_ast(code->args, indent + 1, s);
            fprintf(s, "\n%*s)", indent * 4, "");
            break;
        case ARGUMENT_ENTRY:
            fprintf(s, "arg_entry(");
            fprintf(s, "%*sname: ", (indent + 1) * 4, "");
            dump_ast(code->name, indent + 1, s);
            fprintf(s, ",\n%*sdefault: ", (indent + 1) * 4, "");
            dump_ast(code->def, indent + 1, s);
            fprintf(s, ",\n%*srest: ", (indent + 1) * 4, "");
            dump_ast(code->rest, indent + 1, s);
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
            fprintf(s, "%*sbindings: ", (indent + 1) * 4, "");
            dump_ast(code->bindings, indent + 1, s);
            fprintf(s, ",\n%*sresult: ", (indent + 1) * 4, "");
            dump_ast(code->result, indent + 1, s);
            fprintf(s, ",\n%*sparent: ", (indent + 1) * 4, "");
            dump_ast(code->parent, indent + 1, s);
            fprintf(s, "\n%*s)", indent * 4, "");
            break;
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

static void register_stdlib(pickle_t vm) {
    PIK_DEBUG_PRINTF("register standard library\n");
}


// Section: REPL

#ifdef PIK_TEST
void repl(pickle_t vm) {
    char* buf = (char*)malloc(64 * sizeof(char));
    size_t sz = 64;
    while (true) {
        printf("pickle> ");
        fflush(stdout);
        if (getline(&buf, &sz, stdin) == -1) {
            printf("^D\n");
            goto done;
        }
        if (!strncmp(buf, "bye", 3)) {
            goto done;
        }
        if (pik_compile(vm, buf, vm->global_scope) == RERROR) {
            printf("Compile error!\n%s\n", vm->global_scope->result->message);
            continue;
        }
        if (pik_eval(vm, vm->global_scope->result, vm->global_scope) == RERROR) {
            printf("Execute error!\n%s\n", vm->global_scope->result->message);
            continue;
        }
        pik_print_to(vm, vm->global_scope->result, stdout);
        putchar('\n');
    }
    done:
    free(buf);
    return;
}

int main() {
    pickle_t vm = pik_new();
    repl(vm);
    pik_destroy(vm);
    return 0;
}
#endif

#ifdef __cplusplus
}
#endif
