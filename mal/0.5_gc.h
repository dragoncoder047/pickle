#ifdef __cplusplus
extern "C" {
#endif

#include <stdlib.h>
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
    SOURCECODE,         // char*       | (empty)   | (empty)
    ERROR,              // char*       | (empty)   | (empty)
    INTEGER,            // int64_t............     | (empty)
    FLOAT,              // double.............     | (empty)
    COMPLEX,            // float       | float     | (empty)
    RATIONAL,           // float       | float     | (empty)
    BUILTIN_FUNCTION,   // func*       | (empty)   | (empty)
    STREAM,             // char*       | FILE*     | (empty)

    // Complex types
    LIST,               // item        | (empty)   | rest
    MAP,                // key         | value     | rest
    CLASS,              // parents     | scope     | namespace
    USER_FUNCTION,      // name        | scope     | arguments

    // Internal types
    ARGUMENT_LIST,      // name        | default   | rest
    GETVAR,             // symbol      | (empty)   | (empty)
    EXPRESSION,         // element     | (empty)   | rest
    CALL,               // name        | args      | (empty)
    LIST_LITERAL,       // element     | (empty)   | rest
    SCOPE,              // bindings    | result    | parent   -- code is in subtype
    BINDING             // char*       | value     | rest
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

typedef struct pik_object* pik_object_t;
struct pik_object {
    // Basic information about the object
    uint16_t type;
    pik_object_t next_object;
    union {
        uint16_t subtype;
        uint16_t retcode;
    };
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
                char* chars, message;
                float real, numerator;
            };
            // Cell 2
            union {
                pik_object_t cell2, cdr, value, scope, def, result, args;
                float imag, denominator;
                FILE* stream;
            };
            // Cell 3
            union {
                pik_object_t cell3, rest, ns, arguments, parent;
            };
        };
    };
};

typedef struct pickle* pickle_t;

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
static void register_primitive_types(pickle_t vm);

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
    CELL1_EMPTY  = 0b000000,
    CELL1_CHARS  = 0b000001,
    CELL1_OBJECT = 0b000010,
    CELL1_MASK   = 0b000011,
    CELL2_EMPTY  = 0b000000,
    CELL2_FILE   = 0b000100,
    CELL2_OBJECT = 0b001000,
    CELL2_MASK   = 0b001100,
    CELL3_EMPTY  = 0b000000,
    CELL3_OBJECT = 0b010000,
    CELL3_MASK   = 0b110000
};
static int type_info(uint16_t type) {
    switch (type) {
        case CONS:              return CELL1_OBJECT | CELL2_OBJECT | CELL3_EMPTY;
        case SYMBOL:            return CELL1_CHARS  | CELL2_EMPTY  | CELL3_EMPTY;
        case STRING:            return CELL1_CHARS  | CELL2_EMPTY  | CELL3_EMPTY;
        case SOURCECODE:        return CELL1_CHARS  | CELL2_EMPTY  | CELL3_EMPTY;
        case ERROR:             return CELL1_CHARS  | CELL2_EMPTY  | CELL3_EMPTY;
        case INTEGER:           return CELL1_EMPTY  | CELL2_EMPTY  | CELL3_EMPTY;
        case FLOAT:             return CELL1_EMPTY  | CELL2_EMPTY  | CELL3_EMPTY;
        case COMPLEX:           return CELL1_EMPTY  | CELL2_EMPTY  | CELL3_EMPTY;
        case RATIONAL:          return CELL1_EMPTY  | CELL2_EMPTY  | CELL3_EMPTY;
        case BUILTIN_FUNCTION:  return CELL1_EMPTY  | CELL2_EMPTY  | CELL3_EMPTY;
        case STREAM:            return CELL1_CHARS  | CELL2_FILE   | CELL3_EMPTY;
        case LIST:              return CELL1_OBJECT | CELL2_EMPTY  | CELL3_OBJECT;
        case MAP:               return CELL1_OBJECT | CELL2_OBJECT | CELL3_OBJECT;
        case CLASS:             return CELL1_OBJECT | CELL2_OBJECT | CELL3_OBJECT;
        case USER_FUNCTION:     return CELL1_OBJECT | CELL2_OBJECT | CELL3_OBJECT;
        case ARGUMENT_LIST:     return CELL1_OBJECT | CELL2_OBJECT | CELL3_OBJECT;
        case GETVAR:            return CELL1_OBJECT | CELL2_EMPTY  | CELL3_EMPTY;
        case EXPRESSION:        return CELL1_OBJECT | CELL2_EMPTY  | CELL3_OBJECT;
        case CALL:              return CELL1_OBJECT | CELL2_OBJECT | CELL3_EMPTY;
        case LIST_LITERAL:      return CELL1_OBJECT | CELL2_EMPTY  | CELL3_OBJECT;
        case SCOPE:             return CELL1_OBJECT | CELL2_OBJECT | CELL3_OBJECT;
        case BINDING:           return CELL1_CHARS  | CELL2_OBJECT | CELL3_OBJECT;
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
    register_primitive_types(vm);
    PIK_DEBUG_PRINTF("For global scope: ");
    vm->global_scope = alloc_object(vm, SCOPE, 0);
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

// Section: Parser

typedef struct pik_parser {
    const char* code;
    size_t len;
    size_t head;
} pik_parser;

pik_object_t pik_compile(pickle_t vm, const char* code) {
    pik_object_t result = alloc_object(vm, STRING, 0);
    result->chars = strdup(code);
    return result;
}

// Section: Evaluator

pik_object_t pik_eval(pickle_t vm, pik_object_t x) {
    pik_incref(x);
    return x;
}

// Section: Printer

void pik_print_string_to(const char* c, FILE* s) {
    fprintf(s, "%s", c);
}

void pik_print_to(pickle_t vm, pik_object_t object, FILE* s) {
    IF_NULL_RETURN(object);
    switch (object->type) {
        case STRING:
            fprintf(s, "%s", object->chars);
    }
}

// Section: Builtin functions

static void register_primitive_types(pickle_t vm) {
    PIK_DEBUG_PRINTF("register primitive types\n");
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
        // remove line terminator
        buf[strlen(buf) - 1] = 0;
        if (!strcmp(buf, "bye")) {
            goto done;
        }
        pik_object_t code = pik_compile(vm, buf);
        pik_object_t result = pik_eval(vm, code);
        pik_print_to(vm, result, stdout);
        pik_decref(vm, result);
        pik_decref(vm, code);
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
