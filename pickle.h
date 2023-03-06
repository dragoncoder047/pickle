#ifdef __cplusplus
extern "C" {
#endif

#include <stdlib.h>
#include <stdio.h>
#include <stdint.h>
#include <string.h>
#include <ctype.h>

// #define PIK_DEBUG
#define PIK_TEST

#pragma GCC optimize ("Os")

#if !defined(bool) && !defined(__cplusplus)
typedef int bool;
#define true 1
#define false 0
#endif

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

typedef uint16_t pik_type;
typedef uint16_t pik_flags;
typedef uint16_t pik_resultcode;
typedef struct pik_object pik_object;
typedef struct pik_vm pik_vm;

typedef pik_object* (*pik_builtin_function)(pik_vm*, pik_object*, pik_object*, pik_object*);

// These function only operate on the void* payload of the object, everything else is handled automatically
typedef void (*pik_type_function)(pik_vm*, pik_object*, void*);
#define PIK_NUMTYPEFUNS 3
#define PIK_INITFUN 0
#define PIK_MARKFUN 1
#define PIK_FINALFUN 2

typedef void (*pik_exit_callback)(pik_vm*, pik_object*);
typedef void (*pik_write_callback)(pik_vm*, const char*);
typedef char* (*pik_read_callback)(pik_vm*, const char*);
typedef char* (*pik_source_callback)(pik_vm*, const char*);
typedef void (*pik_store_callback)(pik_vm*, const char*, const char*);
typedef void (*pik_error_callback)(pik_vm*, size_t, const char*);
typedef const char* (*pik_embeddedfilter_callback)(pik_vm*, const char*);
typedef const char* (*pik_checkinterrupt_callback)(pik_vm*);

#define streq(a, b) (!strcmp((a), (b)))
#define IF_NULL_RETURN(x) if ((x) == NULL) return

// Types
#define PIK_TYPE_NONE 0
#define PIK_TYPE_INT 1
#define PIK_TYPE_FLOAT 2
#define PIK_TYPE_COMPLEX 3
#define PIK_TYPE_BOOL 4
#define PIK_TYPE_ERROR 5
#define PIK_TYPE_STRING 6
#define PIK_TYPE_LIST 7
#define PIK_TYPE_DICT 8
#define PIK_TYPE_FUNCTION 9
#define PIK_TYPE_CLASS 10
#define PIK_TYPE_CODE 11
#define PIK_TYPE_SCOPE 12

// Global Flags
#define PIK_MARKED 1
#define PIK_ALREADY_FINALIZED 2

// Object Flags
#define PIK_FUNCTION_FLAG_IS_USER 1

// Code Types
#define PIK_CODE_BLOCK 0
#define PIK_CODE_LINE 1
#define PIK_CODE_WORD 2
#define PIK_CODE_OPERATOR 3
#define PIK_CODE_GETVAR 4
#define PIK_CODE_CONCAT 5
#define PIK_CODE_LIST 6

#define PIK_HASHMAP_BUCKETS 256
#define PIK_HASHMAP_BUCKETMASK 0xFF
typedef struct pik_hashentry {
    char* key;
    pik_object* value;
    bool read_only;
} pik_hashentry;

typedef struct pik_hashbucket {
    pik_hashentry* entries;
    size_t sz;
} pik_hashbucket;

typedef struct pik_hashmap {
    pik_hashbucket buckets[PIK_HASHMAP_BUCKETS];
} pik_hashmap;

typedef struct pik_complex {
    float real;
    float imag;
} pik_complex;

typedef struct pik_userfunc {
    pik_object* code;
    pik_object* scope;
    char* name;
    char** argnames;
    size_t argc;
} pik_userfunc;

struct pik_object {
    pik_type type;
    struct {
        pik_object* next;
        size_t refcnt;
    } gc;
    struct {
        pik_flags global;
        pik_flags obj;
    } flags;
    struct {
        pik_object** bases;
        size_t sz;
    } proto;
    pik_hashmap* properties;
    union {
        void* as_pointer;
        char* as_chars;
        struct {
            void* car;
            void* cdr;
        } as_cons;
        int64_t as_int;
        double as_double;
        pik_complex as_complex;
        pik_object* as_object;
        struct {
            pik_object** items;
            size_t sz;
        } as_array;
        pik_hashmap* as_hashmap;
        pik_builtin_function as_c_function;
        pik_userfunc* as_userfunc;
    } payload;
};

typedef struct pik_typemgr {
    pik_type_function funs[PIK_NUMTYPEFUNS];
    pik_type type_for;
    const char* type_string;
    const pik_object* prototype;
} pik_typemgr;

typedef struct pik_operator {
    const char* symbol;
    const char* method;
    int precedence;
} pik_operator;

typedef struct pik_parser {
    const char* code;
    size_t len;
    size_t head;
} pik_parser;

struct pik_vm {
    struct {
        pik_object* first;
        size_t num_objects;
    } gc;
    struct {
        pik_typemgr* mgrs;
        size_t sz;
    } type_managers;
    struct {
        pik_operator* ops;
        size_t sz;
    } operators;
    pik_object* global_scope;
    pik_object* dollar_function;
    pik_object* error;
    struct {
        pik_exit_callback exit;
        pik_write_callback write;
        pik_read_callback read;
        pik_source_callback source;
        pik_store_callback store;
        pik_error_callback error;
        pik_embeddedfilter_callback embeddedfilter;
        pik_checkinterrupt_callback checkinterrupt;
    } callbacks;
};

#define PIK_DOCALLBACK(vm, name, ...) \
if (vm->callbacks.name) { \
    vm->callbacks.name(vm, __VA_ARGS__);\
}

// Forward references
void pik_register_globals(pik_vm*);
static void register_primitive_types(pik_vm*);
void pik_incref(pik_object*);
void pik_decref(pik_vm*, pik_object*);
static void mark_object(pik_vm*, pik_object*);
static pik_hashmap* new_hashmap(void);
static void hashmap_destroy(pik_vm*, pik_hashmap*);
static pik_object* next_item(pik_vm*, pik_parser*);
static pik_object* eval_line(pik_vm*, pik_object*, pik_object*, pik_object*, pik_object*);
static pik_object* eval_user_code(pik_vm*, pik_object*, pik_object*, pik_object*, pik_object*);

// ------------------- Basic objects -------------------------

const char* pik_typename(pik_vm* vm, pik_type type) {
    for (size_t i = 0; i < vm->type_managers.sz; i++) { 
        if (vm->type_managers.mgrs[i].type_for == type) { 
            return vm->type_managers.mgrs[i].type_string;
        }
    }
    return "object";
}

const char* pik_typeof(pik_vm* vm, pik_object* object) {
    IF_NULL_RETURN(object) "null";
    return pik_typename(vm, object->type);
}

static void run_typefun(pik_vm* vm, pik_object* object, void* arg, int name) {
    pik_type type = object->type;
    for (size_t i = 0; i < vm->type_managers.sz; i++) { 
        if (vm->type_managers.mgrs[i].type_for == type && vm->type_managers.mgrs[i].funs[name] != NULL) { 
            vm->type_managers.mgrs[i].funs[name](vm, object, arg); 
            return; 
        }
    }
}

static pik_object* alloc_object(pik_vm* vm, pik_type type, void* arg) {
    pik_object* object = vm->gc.first;
    while (object != NULL) {
        if (object->gc.refcnt == 0) {
            PIK_DEBUG_PRINTF("Reusing garbage");
            goto got_unused;
        }
        object = object->gc.next;
    }
    PIK_DEBUG_PRINTF("Allocating new memory ");
    object = (pik_object*)calloc(1, sizeof(pik_object));
    PIK_DEBUG_PRINTF("at %p", (void*)object);
    object->gc.next = vm->gc.first;
    vm->gc.first = object;
    vm->gc.num_objects++;
    got_unused:
    PIK_DEBUG_PRINTF(" for a %s\n", pik_typename(vm, type));
    object->type = type;
    object->gc.refcnt = 1;
    object->properties = new_hashmap();
    object->flags.obj = 0;
    object->flags.global = 0;
    run_typefun(vm, object, arg, PIK_INITFUN);
    return object;
}

void pik_add_prototype(pik_object* object, pik_object* proto) {
    IF_NULL_RETURN(object);
    IF_NULL_RETURN(proto);
    for (size_t i = 0; i < object->proto.sz; i++) {
        if (object->proto.bases[i] == proto) return; // Don't add the same prototype twice
    }
    object->proto.bases = (pik_object**)realloc(object->proto.bases, (object->proto.sz + 1) * sizeof(pik_object*));
    object->proto.bases[object->proto.sz] = proto;
    object->proto.sz++;
    pik_incref(proto);
}

pik_object* pik_create(pik_vm* vm, pik_type type, pik_object* proto, void* arg) {
    pik_object* object = alloc_object(vm, type, arg);
    pik_add_prototype(object, proto);
    return object;
}

pik_object* pik_create_primitive(pik_vm* vm, pik_type type, void* arg) {
    // todo: string and number interning
    pik_object* proto = NULL;
    for (size_t i = 0; i < vm->type_managers.sz; i++) { 
        if (vm->type_managers.mgrs[i].type_for == type && vm->type_managers.mgrs[i].prototype != NULL) { 
            proto = (pik_object*)vm->type_managers.mgrs[i].prototype; 
            break;
        }
    }
    return pik_create(vm, type, proto, arg);
}

inline void pik_incref(pik_object* object) {
    IF_NULL_RETURN(object);
    object->gc.refcnt++;
    PIK_DEBUG_PRINTF("object %p got a new reference (now have %zu)\n", (void*)object, object->gc.refcnt);
}

static void finalize(pik_vm* vm, pik_object* object) {
    IF_NULL_RETURN(object);
    if (object->flags.global & PIK_ALREADY_FINALIZED) {
        PIK_DEBUG_PRINTF("Already finalized %s at %p\n", pik_typeof(vm, object), (void*)object);
        return;
    }
    PIK_DEBUG_PRINTF("Finalizing %s at %p\n", pik_typeof(vm, object), (void*)object);
    // Free object-specific stuff
    run_typefun(vm, object, NULL, PIK_FINALFUN);
    #ifdef PIK_DEBUG
    if (object->payload.as_int != 0) {
        printf("warning: object at %p (a %s) payload was not cleared by finalizer\n", (void*)object, pik_typeof(vm, object));
    }
    #endif
    // Free everything else
    object->flags.global = PIK_ALREADY_FINALIZED;
    object->flags.obj = 0;
    for (size_t i = 0; i < object->proto.sz; i++) {
        pik_decref(vm, object->proto.bases[i]);
    }
    free(object->proto.bases);
    object->proto.bases = NULL;
    object->proto.sz = 0;
    hashmap_destroy(vm, object->properties);
    object->properties = NULL;
}

void pik_decref(pik_vm* vm, pik_object* object) {
    IF_NULL_RETURN(object);
    PIK_DEBUG_ASSERT(object->gc.refcnt > 0, "Decref'ed an object with 0 references");
    object->gc.refcnt--;
    if (object->gc.refcnt == 0) {
        PIK_DEBUG_PRINTF("%s at %p lost all references, finalizing\n", pik_typeof(vm, object), (void*)object);
        // Free it now, no other references
        finalize(vm, object);
        // Unmark it so it will be collected if a GC is ongoing
        object->flags.global &= ~PIK_MARKED;
    }
    #ifdef PIK_DEBUG
    else {
        printf("%s at %p lost a reference (now have %zu)\n", pik_typeof(vm, object), (void*)object, object->gc.refcnt);
    }
    #endif
}

static void mark_hashmap(pik_vm* vm, pik_hashmap* map) {
    for (size_t i = 0; i < PIK_HASHMAP_BUCKETS; i++) {
        pik_hashbucket b = map->buckets[i];
        #ifdef PIK_DEBUG
        if (b.sz > 0) printf("map->buckets[%zu]\n", i);
        #endif
        for (size_t j = 0; j < b.sz; j++) {
            mark_object(vm, b.entries[i].value);
        }
    }
}

static void mark_object(pik_vm* vm, pik_object* object) {
    mark:
    PIK_DEBUG_PRINTF("Marking %s at %p:\n", pik_typeof(vm, object), (void*)object);
    if (object == NULL || object->flags.global & PIK_MARKED) return;
    object->flags.global |= PIK_MARKED;
    // Mark payload
    PIK_DEBUG_PRINTF("%p->payload\n", (void*)object);
    run_typefun(vm, object, NULL, PIK_MARKFUN);
    // Mark properties
    PIK_DEBUG_PRINTF("%p->properties\n", (void*)object);
    mark_hashmap(vm, object->properties);
    // Mark prototypes
    if (object->proto.sz == 0) {
        PIK_DEBUG_PRINTF("%p->prototype == null\n", (void*)object);
        return;
    }
    // - 1 for tail-call optimization
    PIK_DEBUG_PRINTF("%p->prototype (%zu bases)\n", (void*)object, object->proto.sz);
    size_t i;
    for (i = 0; i < object->proto.sz - 1; i++) {
        mark_object(vm, object->proto.bases[i]);
    }
    // Tail-call optimize
    object = object->proto.bases[i]; // i is now == object->proto.sz - 1
    goto mark;
}

static void sweep_unmarked(pik_vm* vm) {
    pik_object** object = &vm->gc.first;
    while (*object != NULL) {
        PIK_DEBUG_PRINTF("Looking at %s at %p: flags=%#x, ", pik_typeof(vm, *object), (void*)(*object), (*object)->flags.global);
        if ((*object)->flags.global & PIK_MARKED) {
            PIK_DEBUG_PRINTF("marked\n");
            // Keep the object
            (*object)->flags.global &= ~PIK_MARKED;
            object = &(*object)->gc.next;
        } else {
            PIK_DEBUG_PRINTF("unmarked\n");
            // Sweep the object
            pik_object* unreached = *object;
            *object = unreached->gc.next;
            finalize(vm, unreached);
            free(unreached);
            vm->gc.num_objects--;
        }
    }
}

size_t pik_collect_garbage(pik_vm* vm) {
    IF_NULL_RETURN(vm) 0;
    PIK_DEBUG_PRINTF("Collecting garbage\n");
    mark_object(vm, vm->global_scope);
    mark_object(vm, vm->dollar_function);
    mark_object(vm, vm->error);
    size_t start = vm->gc.num_objects;
    sweep_unmarked(vm);
    size_t freed = start - vm->gc.num_objects;
    PIK_DEBUG_PRINTF("%zu freed, %zu objects remaining after gc\n", freed, vm->gc.num_objects);
    return freed;
}

pik_vm* pik_new(void) {
    pik_vm* vm = (pik_vm*)calloc(1, sizeof(pik_vm));
    register_primitive_types(vm);
    PIK_DEBUG_PRINTF("For global scope: ");
    vm->global_scope = alloc_object(vm, PIK_TYPE_NONE, NULL);
    // TODO: register global functions
    return vm;
}

void pik_destroy(pik_vm* vm) {
    IF_NULL_RETURN(vm);
    PIK_DEBUG_PRINTF("Freeing the VM - garbage collect all: ");
    vm->global_scope = NULL;
    vm->dollar_function = NULL;
    vm->error = NULL;
    pik_collect_garbage(vm);
    PIK_DEBUG_ASSERT(vm->gc.first == NULL, "Garbage collection failed to free all objects");
    PIK_DEBUG_PRINTF("Freeing %zu type managers\n", vm->type_managers.sz);
    free(vm->type_managers.mgrs);
    PIK_DEBUG_PRINTF("Freeing %zu operators\n", vm->operators.sz);
    free(vm->operators.ops);
    PIK_DEBUG_PRINTF("Freeing VM\n");
    free(vm);
}

void pik_register_type(pik_vm* vm, pik_type type, const char* name, pik_type_function init, pik_type_function mark, pik_type_function finalize) {
    vm->type_managers.mgrs = (pik_typemgr*)realloc(vm->type_managers.mgrs, (vm->type_managers.sz + 1) * sizeof(pik_typemgr));
    vm->type_managers.mgrs[vm->type_managers.sz].type_for = type;
    vm->type_managers.mgrs[vm->type_managers.sz].type_string = name;
    vm->type_managers.mgrs[vm->type_managers.sz].funs[PIK_INITFUN] = init;
    vm->type_managers.mgrs[vm->type_managers.sz].funs[PIK_MARKFUN] = mark;
    vm->type_managers.mgrs[vm->type_managers.sz].funs[PIK_FINALFUN] = finalize;
    vm->type_managers.sz++;
    PIK_DEBUG_PRINTF("Registered type #%hu: %s\n", type, name);
}

void pik_set_default_prototype(pik_vm* vm, pik_type type, pik_object* prototype) {
    for (size_t i = 0; i < vm->type_managers.sz; i++) {
        if (vm->type_managers.mgrs[i].type_for == type) {
            vm->type_managers.mgrs[i].prototype = prototype;
            PIK_DEBUG_PRINTF("object at %p is now the default prototype for type %s\n", (void*)prototype, pik_typename(vm, type));
            pik_incref(prototype);
            return; 
        }
    }
}

void pik_set_error(pik_vm* vm, const char* message) {
    pik_decref(vm, vm->error);
    vm->error = alloc_object(vm, PIK_TYPE_ERROR, (void*)message);
}
#define pik_set_error_fmt(vm, ...) do { char* temp_buf_; asprintf(&temp_buf_, __VA_ARGS__); pik_set_error(vm, temp_buf_); free(temp_buf_); } while (0)


// ------------------------- Hashmap stuff -------------------------

static inline pik_hashmap* new_hashmap(void) {
    return (pik_hashmap*)calloc(1, sizeof(pik_hashmap));
}

static unsigned int hashmap_hash(const char* value) {
    unsigned int hash = 5381;
    size_t len = strlen(value);
    PIK_DEBUG_PRINTF("Hash of \"%s\" (%zu chars) -> ", value, len);
    for (size_t i = 0; i < len; i++) {
        hash = (hash << 5) + hash + value[i];
    }
    hash &= PIK_HASHMAP_BUCKETMASK;
    PIK_DEBUG_PRINTF("%u\n", hash);
    return hash;
}

static void hashmap_destroy(pik_vm* vm, pik_hashmap* map) {
    IF_NULL_RETURN(map);
    PIK_DEBUG_PRINTF("Freeing hashmap at %p\n", (void*)map);
    for (size_t b = 0; b < PIK_HASHMAP_BUCKETS; b++) {
        pik_hashbucket bucket = map->buckets[b];
        #ifdef PIK_DEBUG
        if (bucket.sz > 0) printf("%zu entries in bucket %zu\n", bucket.sz, b);
        #endif
        for (size_t e = 0; e < bucket.sz; e++) {
            free(bucket.entries[e].key);
            pik_decref(vm, bucket.entries[e].value);
        }
    }
    free(map);
}

static pik_hashbucket* get_bucket(pik_hashmap* map, const char* key) {
    IF_NULL_RETURN(map) NULL;
    unsigned int hash = hashmap_hash(key);
    return &map->buckets[hash];
}

static pik_hashentry* find_entry(pik_hashbucket* b, const char* key) {
    IF_NULL_RETURN(b) NULL;
    PIK_DEBUG_PRINTF("Checking %zu existing entries in bucket:\n", b->sz);
    for (size_t i = 0; i < b->sz; i++) {
        if (streq(b->entries[i].key, key)) {
            PIK_DEBUG_PRINTF("found key %s\n", key);
            return &b->entries[i];
        }
    }
    PIK_DEBUG_PRINTF("key %s not found\n", key);
    return NULL;
}

void pik_hashmap_put(pik_vm* vm, pik_hashmap* map, const char* key, pik_object* value, bool read_only) {
    PIK_DEBUG_PRINTF("Adding %s at %p to hashmap at key \"%s\"%s\n", pik_typeof(vm, value), (void*)value, key, read_only ? " (read_only)" : "");
    IF_NULL_RETURN(map);
    pik_incref(value);
    pik_hashbucket* b = get_bucket(map, key);
    pik_hashentry* e = find_entry(b, key);
    if (e) {
        PIK_DEBUG_PRINTF("used old entry for %s\n", key);
        pik_decref(vm, e->value);
        e->value = value;
        e->read_only = read_only;
        return;
    }
    PIK_DEBUG_PRINTF("new entry for %s\n", key);
    b->entries = (pik_hashentry*)realloc(b->entries, sizeof(pik_hashentry) * (b->sz + 1));
    b->entries[b->sz].key = strdup(key);
    b->entries[b->sz].value = value;
    b->entries[b->sz].read_only = read_only;
    b->sz++;
}

pik_object* pik_hashmap_get(pik_hashmap* map, const char* key) {
    PIK_DEBUG_PRINTF("Getting entry %s on hashmap %p\n", key, (void*)map);
    IF_NULL_RETURN(map) NULL;
    pik_hashentry* e = find_entry(get_bucket(map, key), key);
    if (e) {
        pik_incref(e->value);
        return e->value;
    }
    return NULL;
}

bool pik_hashmap_entry_is_read_only(pik_hashmap* map, const char* key) {
    PIK_DEBUG_PRINTF("Getting lock bit of %s on hashmap %p\n", key, (void*)map);
    IF_NULL_RETURN(map) false;
    pik_hashentry* e = find_entry(get_bucket(map, key), key);
    if (e) {
        PIK_DEBUG_PRINTF("%s\n", e->read_only ? "read_only" : "writable");
        return e->read_only;
    }
    return false;
}

bool pik_hashmap_has(pik_hashmap* map, const char* key) {
    PIK_DEBUG_PRINTF("Testing presence of key: ");
    return !!find_entry(get_bucket(map, key), key);
}

// ------------------------------------- Builtin primitive types ----------------------------------

void pik_tf_STRDUP_ARG(pik_vm* vm, pik_object* object, void* arg) {
    (void)vm;
    object->payload.as_chars = strdup((char*)arg);
}

void pik_tf_FREE_PAYLOAD(pik_vm* vm, pik_object* object, void* arg) {
    (void)vm;
    (void)arg;
    free(object->payload.as_pointer);
    object->payload.as_pointer = NULL;
}

void pik_tf_NOOP(pik_vm* vm, pik_object* object, void* arg) {
    (void)vm;
    (void)object;
    (void)arg;
}

void pik_tf_COPY_PTR(pik_vm* vm, pik_object* object, void* arg) {
    (void)vm;
    object->payload.as_pointer = arg;
}

void pik_tf_COPY_ARG_BITS(pik_vm* vm, pik_object* object, void* arg) {
    (void)vm;
    object->payload.as_int = *(int64_t*)arg;
}

void pik_tf_FREE_IF_USER_CODE(pik_vm* vm, pik_object* object, void* arg) {
    (void)arg;
    if (object->flags.obj & PIK_FUNCTION_FLAG_IS_USER) {
        pik_decref(vm, object->payload.as_userfunc->code);
        pik_decref(vm, object->payload.as_userfunc->scope);
        for (size_t i = 0; i < object->payload.as_userfunc->argc; i++) {
            free(object->payload.as_userfunc->argnames[i]);
        }
        free(object->payload.as_userfunc->argnames);
        free(object->payload.as_userfunc->name);
        free(object->payload.as_userfunc);
        object->payload.as_userfunc = NULL;
    }
}

void pik_tf_MARK_ITEMS(pik_vm* vm, pik_object* object, void* arg) {
    (void)arg;
    IF_NULL_RETURN(object->payload.as_array.items);
    for (size_t i = 0; i < object->payload.as_array.sz; i++) {
        mark_object(vm, object->payload.as_array.items[i]);
    }
}

void pik_tf_FREE_ITEMS(pik_vm* vm, pik_object* object, void* arg) {
    (void)arg;
    IF_NULL_RETURN(object->payload.as_array.items);
    for (size_t i = 0; i < object->payload.as_array.sz; i++) {
        pik_decref(vm, object->payload.as_array.items[i]);
    }
    free(object->payload.as_array.items);
    object->payload.as_array.items = NULL;
    object->payload.as_array.sz = 0;
}

void pik_tf_init_hashmap(pik_vm* vm, pik_object* object, void* arg) {
    (void)vm;
    (void)arg;
    object->payload.as_hashmap = new_hashmap();
}

void pik_tf_mark_hashmap(pik_vm* vm, pik_object* object, void* arg) {
    (void)arg;
    mark_hashmap(vm, object->payload.as_hashmap);
}

void pik_tf_free_hashmap(pik_vm* vm, pik_object* object, void* arg) {
    (void)arg;
    hashmap_destroy(vm, object->payload.as_hashmap);
    object->payload.as_hashmap = NULL;
}

void pik_tf_init_code(pik_vm* vm, pik_object* object, void* arg) {
    (void)vm;
    pik_flags ct = *(pik_flags*)arg;
    object->flags.obj = ct;
}

void pik_tf_mark_code(pik_vm* vm, pik_object* object, void* arg) {
    (void)arg;
    pik_flags ct = object->flags.obj;
    switch (ct) {
        case PIK_CODE_WORD:
        case PIK_CODE_GETVAR:
        case PIK_CODE_OPERATOR:
            break;
        case PIK_CODE_CONCAT:
        case PIK_CODE_LINE:
        case PIK_CODE_BLOCK:
        case PIK_CODE_LIST:
            pik_tf_MARK_ITEMS(vm, object, NULL);
    }
}

void pik_tf_free_code(pik_vm* vm, pik_object* object, void* arg) {
    (void)arg;
    pik_flags ct = object->flags.obj;
    object->flags.obj = 0;
    switch (ct) {
        case PIK_CODE_WORD:
        case PIK_CODE_GETVAR:
        case PIK_CODE_OPERATOR:
            free(object->payload.as_pointer);
            object->payload.as_pointer = NULL;
            break;
        case PIK_CODE_CONCAT:
        case PIK_CODE_LINE:
        case PIK_CODE_BLOCK:
        case PIK_CODE_LIST:
            pik_tf_FREE_ITEMS(vm, object, NULL);
            break;
    }
}

static void register_primitive_types(pik_vm* vm) {
    PIK_DEBUG_PRINTF("Registering builtin types\n");
    pik_register_type(vm, PIK_TYPE_INT, "int", pik_tf_COPY_ARG_BITS, pik_tf_NOOP, pik_tf_NOOP);
    pik_register_type(vm, PIK_TYPE_FLOAT, "float", pik_tf_COPY_ARG_BITS, pik_tf_NOOP, pik_tf_NOOP);
    pik_register_type(vm, PIK_TYPE_BOOL, "bool", pik_tf_COPY_ARG_BITS, pik_tf_NOOP, pik_tf_NOOP);
    pik_register_type(vm, PIK_TYPE_COMPLEX, "complex", pik_tf_COPY_ARG_BITS, pik_tf_NOOP, pik_tf_NOOP);
    pik_register_type(vm, PIK_TYPE_ERROR, "error", pik_tf_STRDUP_ARG, pik_tf_NOOP, pik_tf_FREE_PAYLOAD);
    pik_register_type(vm, PIK_TYPE_STRING, "str", pik_tf_STRDUP_ARG, pik_tf_NOOP, pik_tf_FREE_PAYLOAD);
    pik_register_type(vm, PIK_TYPE_LIST, "list", pik_tf_NOOP, pik_tf_MARK_ITEMS, pik_tf_FREE_ITEMS);
    pik_register_type(vm, PIK_TYPE_DICT, "dict", pik_tf_init_hashmap, pik_tf_mark_hashmap, pik_tf_free_hashmap);
    pik_register_type(vm, PIK_TYPE_FUNCTION, "function", pik_tf_COPY_PTR, pik_tf_NOOP, pik_tf_FREE_IF_USER_CODE);
    pik_register_type(vm, PIK_TYPE_CLASS, "class", pik_tf_COPY_PTR, pik_tf_NOOP, pik_tf_FREE_IF_USER_CODE);
    pik_register_type(vm, PIK_TYPE_CODE, "code", pik_tf_init_code, pik_tf_mark_code, pik_tf_free_code);
    pik_register_type(vm, PIK_TYPE_SCOPE, "_internal_scope", pik_tf_NOOP, pik_tf_NOOP, pik_tf_NOOP);
}

void pik_APPEND_INPLACE(pik_object* a, pik_object* item) {
    IF_NULL_RETURN(a);
    a->payload.as_array.items = (pik_object**)realloc(a->payload.as_array.items, (a->payload.as_array.sz + 1) * sizeof(pik_object*));
    a->payload.as_array.items[a->payload.as_array.sz] = item;
    a->payload.as_array.sz++;
    pik_incref(item);
}

static inline pik_object* create_codeobj(pik_vm* vm, pik_flags codetype) {
    return pik_create_primitive(vm, PIK_TYPE_CODE, (void*)&codetype);
}

// ------------------------------- Parser ------------------------------

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
    if (c == ';') return true;
    return c == '\n' || c == '\r';
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

static pik_object* get_getvar(pik_vm* vm, pik_parser* p) {
    PIK_DEBUG_PRINTF("get_getvar()\n");
    next(p); // Skip $
    if (!valid_varchar(at(p))) {
        pik_set_error_fmt(vm, "syntax error: \"%s%c\" not allowed after \"$\"", needs_escape(at(p)) ? "\\" : "", escape(at(p)));
        return NULL;
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
    pik_object* gv = create_codeobj(vm, PIK_CODE_GETVAR);
    asprintf(&gv->payload.as_chars, "%.*s", (int)len, str_of(p));
    restore(p, end);
    return gv;
}

static pik_object* get_string(pik_vm* vm, pik_parser* p) {
    char q = at(p);
    next(p);
    if (p_eof(p)) {
        char iq = q == '"' ? '\'' : '"';
        pik_set_error_fmt(vm, "syntax error: dangling %c%c%c", iq, q, iq);
        return NULL;
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
            pik_set_error_fmt(vm, "syntax error: unterminated string %.20s...", str_of(p));
            return NULL;
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
    pik_object* stringobj = pik_create_primitive(vm, PIK_TYPE_STRING, (void*)buf);
    free(buf);
    return stringobj;
}

static pik_object* get_brace_string(pik_vm* vm, pik_parser* p) {
    PIK_DEBUG_PRINTF("get_brace_string()\n");
    next(p); // Skip {
    if (p_eof(p)) {
        pik_set_error(vm, "syntax error: dangling \"{\"");
        return NULL;
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
            pik_set_error_fmt(vm, "syntax error: unbalanced curlies: %.20s...", str_of(p));
            return NULL;
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
    pik_object* str = pik_create_primitive(vm, PIK_TYPE_STRING, (void*)buf);
    restore(p, end);
    free(buf);
    return str;
}

static pik_object* get_colon_string(pik_vm* vm, pik_parser* p) {
    PIK_DEBUG_PRINTF("get_colon_string()\n");
    while (at(p) != '\n') next(p); // Skip all before newline
    next(p); // Skip newline
    size_t indent = 0;
    bool spaces = at(p) == ' ';
    // Find first indent
    while (isspace(at(p))) {
        if (p_eof(p)) {
            pik_set_error(vm, "syntax error: expected indented block after \":\"");
            return NULL;
        }
        if ((!spaces && at(p) == ' ') || (spaces && at(p) == '\t')) {
            pik_set_error(vm, "syntax error: mix of tabs and spaces indenting block");
            return NULL;
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
                pik_set_error(vm, "syntax error: mix of tabs and spaces indenting block");
                return NULL;
            }
            this_indent++;
            next(p);
        }
        if (this_indent > 0 && this_indent < indent) {
            pik_set_error(vm, "syntax error: unindent does not match previous indent");
            return NULL;
        }
        if (this_indent < indent) {
            // & at end means the next items are part of the same line
            if (at(p) != '&') {
                restore(p, last_nl);
            } else next(p);
            break;
        }
    }
    // Go back and get the string
    size_t end = save(p);
    restore(p, start);
    char* buf = (char*)calloc(len + 1, sizeof(char));
    for (size_t i = 0; i < len; i++) {
        buf[i] = at(p);
        if (at(p) == '\n') advance(p, indent); // Skip the indent
        next(p);
    }
    pik_object* str = pik_create_primitive(vm, PIK_TYPE_STRING, (void*)buf);
    restore(p, end);
    free(buf);
    return str;
}

static pik_object* get_expression(pik_vm* vm, pik_parser* p) {
    PIK_DEBUG_PRINTF("get_expression()\n");
    next(p); // Skip (
    pik_object* expr = create_codeobj(vm, PIK_CODE_LINE);
    while (true) {
        if (at(p) == ')') {
            next(p); // Skip )
            break;
        }
        pik_object* next = next_item(vm, p);
        if (vm->error) return NULL;
        if (next) {
            pik_APPEND_INPLACE(expr, next);
            pik_decref(vm, next);
        }
        #ifdef PIK_DEBUG
        else printf("Empty subexpr line\n");
        #endif
    }
    return expr;
}

static pik_object* get_list(pik_vm* vm, pik_parser* p) {
    PIK_DEBUG_PRINTF("get_list()\n");
    next(p); // Skip [
    pik_object* list = create_codeobj(vm, PIK_CODE_LIST);
    while (true) {
        if (at(p) == ']') {
            next(p); // Skip ]
            break;
        }
        pik_object* next = next_item(vm, p);
        if (vm->error) return NULL;
        if (next) {
            pik_APPEND_INPLACE(list, next);
            pik_decref(vm, next);
        }
        #ifdef PIK_DEBUG
        else printf("Empty list line\n");
        #endif
    }
    return list;
}

static pik_object* get_word(pik_vm* vm, pik_parser* p) {
    PIK_DEBUG_PRINTF("get_word()\n");
    size_t len = 0;
    // Special case: boolean
    if (p_startswith(p, "true") || p_startswith(p, "false")) {
        int64_t truthy = at(p) == 't';
        size_t start = save(p);
        advance(p, truthy ? 4 : 5);
        if (isspace(at(p)) || ispunct(at(p))) return pik_create_primitive(vm, PIK_TYPE_BOOL, (void*)&truthy);
        else restore(p, start);
    }
    // Special case: numbers
    if (isdigit(at(p))) {
        char c;
        // Complex
        union { struct { float real; float imag; }; uint64_t complexbits; } foo;
        if (sscanf(str_of(p), "%g%gj%ln%c", &foo.real, &foo.imag, &len, &c) == 3) {
            advance(p, len);
            PIK_DEBUG_PRINTF("complex %g %+g * i (bits = %#llx)\n", foo.real, foo.imag, foo.complexbits);
            return pik_create_primitive(vm, PIK_TYPE_COMPLEX, (void*)&foo.complexbits);
        }
        // Integer
        int64_t intnum;
        if (sscanf(str_of(p), "%lli%ln%c", &intnum, &len, &c) == 2) {
            advance(p, len);
            PIK_DEBUG_PRINTF("integer %lli\n", intnum);
            return pik_create_primitive(vm, PIK_TYPE_INT, (void*)&intnum);
        }
        // Float
        double floatnum;
        if (sscanf(str_of(p), "%lg%ln%c", &floatnum, &len, &c) == 2) {
            advance(p, len);
            PIK_DEBUG_PRINTF("float %lg\n", floatnum);
            return pik_create_primitive(vm, PIK_TYPE_FLOAT, (void*)&floatnum);
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
    pik_object* w = create_codeobj(vm, ispunct(at(p)) ? PIK_CODE_OPERATOR : PIK_CODE_WORD);
    asprintf(&w->payload.as_chars, "%.*s", (int)len, str_of(p));
    restore(p, end);
    return w;
}

static pik_object* next_item(pik_vm* vm, pik_parser* p) {
    IF_NULL_RETURN(vm) NULL;
    IF_NULL_RETURN(p) NULL;
    bool concatenated = false;
    pik_object* result = NULL;
    PIK_DEBUG_PRINTF("next_item()\n");
    again:;
    bool hadspace = skip_whitespace(p);
    if (hadspace && result) return result;
    if (p_eof(p)) return result;
    pik_object* next;
    size_t here = save(p);
    switch (at(p)) {
        case '$':  next = get_getvar(vm, p); break;
        case '"':  // fallthrough
        case '\'': next = get_string(vm, p); break;
        case '{':  next = get_brace_string(vm, p); break;
        case '(':  next = get_expression(vm, p); break;
        case '[':  next = get_list(vm, p); break;
        case ']':  // fallthrough
        case ')':  return result; // allow get_expression() and get_list() to see their end
        case '}':  pik_set_error(vm, "syntax error: unexpected \"}\""); break;
        case ':':  if (result) return result; /* colon blocks are always their own item */ else if (strchr("\n\r", peek(p, 1))) { next = get_colon_string(vm, p); break; } // else fallthrough
        default:   if (eolchar(at(p))) return result; else next = get_word(vm, p); break;
    }
    if (!vm->error && save(p) == here) {
        // Generic failed to parse message
        pik_set_error_fmt(vm, "syntax error: failed to parse: %.20s...", str_of(p));
    }
    if (vm->error) {
        pik_decref(vm, result);
        return NULL;
    }
    if (result == NULL) {
        result = next;
        goto again;
    } else if (!concatenated) {
        pik_object* c = create_codeobj(vm, PIK_CODE_CONCAT);
        pik_APPEND_INPLACE(c, result);
        pik_APPEND_INPLACE(c, next);
        pik_decref(vm, result);
        pik_decref(vm, next);
        result = c;
        concatenated = true;
        goto again;
    } else {
        pik_APPEND_INPLACE(result, next);
        pik_decref(vm, next);
        goto again;
    }
    return result;
}

static pik_object* compile_block(pik_vm* vm, pik_parser* p) {
    IF_NULL_RETURN(vm) NULL;
    IF_NULL_RETURN(p) NULL;
    if (p_eof(p)) return NULL;
    PIK_DEBUG_PRINTF("Begin compile\n");
    pik_object* block = create_codeobj(vm, PIK_CODE_BLOCK);
    while (!p_eof(p)) {
        PIK_DEBUG_PRINTF("Beginning of line: ");
        pik_object* line = create_codeobj(vm, PIK_CODE_LINE);
        while (!p_eof(p)) {
            PIK_DEBUG_PRINTF("Beginning of item: ");
            pik_object* item = next_item(vm, p);
            if (item) pik_APPEND_INPLACE(line, item);
            pik_decref(vm, item);
            if (vm->error) {
                pik_decref(vm, line);
                pik_decref(vm, block);
                return NULL;
            }
            if (eolchar(at(p))) {
                next(p);
                break;
            }
        }
        if (line->payload.as_array.sz > 0) {
            pik_APPEND_INPLACE(block, line);
            pik_decref(vm, line);
        }
        #ifdef PIK_DEBUG
        else printf("Empty line\n");
        #endif
    }
    return block;
}

// ---------------------------------- Object manipulation --------------------------------

bool pik_set_property(pik_vm* vm, pik_object* object, const char* property, pik_object* value, bool read_only) {
    if (pik_hashmap_entry_is_read_only(object->properties, property)) return false;
    pik_hashmap_put(vm, object->properties, property, value, read_only);
    return true;
}

pik_object* pik_get_property(pik_object* object, const char* property) {
    pik_object* x = NULL;
    x = pik_hashmap_get(object->properties, property);
    if (x) goto done;
    for (size_t i = 0; i < object->proto.sz; i++) {
        x = pik_get_property(object->proto.bases[i], property);
        if (x) break;
    }
    done:
    pik_incref(x);
    return x;
}

// ----------------------------------- Operator stuff -----------------------------

static pik_operator* get_op(pik_vm* vm, const char* op) {
    IF_NULL_RETURN(vm) NULL;
    for (size_t i = 0; i < vm->operators.sz; i++) {
        if (streq(vm->operators.ops[i].symbol, op)) return &vm->operators.ops[i];
    }
    return NULL;
}

static int op_precedence(pik_vm* vm, const char* op) {
    IF_NULL_RETURN(vm) 0;
    pik_operator* opinfo = get_op(vm, op);
    IF_NULL_RETURN(opinfo) 0;
    return opinfo->precedence;
}

static const char* op_method(pik_vm* vm, const char* op) {
    IF_NULL_RETURN(vm) 0;
    pik_operator* opinfo = get_op(vm, op);
    IF_NULL_RETURN(opinfo) 0;
    return opinfo->method;
}

// ------------------------------------- Evaluate ------------------------------------

pik_object* pik_get_var(pik_vm* vm, pik_object* scope, const char* var, pik_object* self, pik_object* args) {
    IF_NULL_RETURN(vm) NULL;
    IF_NULL_RETURN(scope) NULL;
    pik_object* result = NULL;
    pik_object* up = NULL;
    if (args && isdigit(var[0])) {
        int num = atoi(var);
        if (num >= args->payload.as_array.sz) {
            pik_set_error_fmt(vm, "undefined numvar $%i (there were only %zu args passed)", num, args->payload.as_array.sz);
            return NULL;
        }
        result = args->payload.as_array.items[num];
        goto done;
    }
    if (strlen(var) == 1) {
        switch (var[0]) {
            case '@': result = args; goto done;
            case '#': result = self; goto done;
            case '?': return pik_hashmap_get(scope->properties, "__last__");
            default:  break;
        }
    }
    result = pik_get_property(scope, var);
    if (result) goto done;
    up = pik_get_property(scope, "__upscope__");
    if (!up) goto error;
    result = pik_get_property(up, var);
    if (!result) goto error;
    done:
    pik_incref(result);
    return result;
    error:
    pik_set_error_fmt(vm, "undefined variable %s", var);
    return NULL;
}

pik_object* pik_call(pik_vm* vm, pik_object* object, pik_object* func, pik_object* args, pik_object* upscope) {
    IF_NULL_RETURN(vm) NULL;
    if (!object) object = vm->global_scope;
    if (!func) func = pik_get_property(object, "__call__");
    if (!func || func->type != PIK_TYPE_FUNCTION) {
        pik_set_error_fmt(vm, "can't call a %s", pik_typeof(vm, object));
        return NULL;
    }
    pik_object* new_scope = pik_create(vm, PIK_TYPE_SCOPE, func->payload.as_userfunc->scope, NULL);
    pik_set_property(vm, new_scope, "__upscope__", upscope, true);
    pik_object* result;
    if (func->flags.global & PIK_FUNCTION_FLAG_IS_USER) {
        if (func->payload.as_userfunc->argc && (!args || func->payload.as_userfunc->argc < args->payload.as_array.sz)) {
            pik_set_error_fmt(vm, "expected %zu args to %s.%s, got %zu", func->payload.as_userfunc->argc, pik_typeof(vm, object), func->payload.as_userfunc->name, args ? args->payload.as_array.sz : 0);
            pik_decref(vm, new_scope);
            return NULL;
        }
        for (size_t i = 0; i < func->payload.as_userfunc->argc; i++) {
            pik_set_property(vm, new_scope, func->payload.as_userfunc->argnames[i], args->payload.as_array.items[i], false);
        }
        result = eval_user_code(vm, object, upscope, func->payload.as_userfunc->code, args);
    } else {
        result = func->payload.as_c_function(vm, object, args, new_scope);
    }
    pik_decref(vm, new_scope);
    return result;

}

pik_object* pik_call_name(pik_vm* vm, pik_object* object, const char* name, pik_object* args, pik_object* upscope) {
    IF_NULL_RETURN(vm) NULL;
    if (!object) object = vm->global_scope;
    pik_object* func = pik_get_property(object, name);
    pik_object* result = pik_call(vm, object, func, args, upscope);
    pik_decref(vm, func); 
    return result;
}

static char* stringify(pik_vm* vm, pik_object* what, pik_object* scope) {
    if (!what) return strdup("NULL");
    char* buf;
    switch (what->type) {
        case PIK_TYPE_STRING: buf = strdup(what->payload.as_chars); break;
        case PIK_TYPE_INT: asprintf(&buf, "%lli", what->payload.as_int); break;
        case PIK_TYPE_FLOAT: asprintf(&buf, "%lg", what->payload.as_double); break;
        case PIK_TYPE_COMPLEX: asprintf(&buf, "%g%+gj", what->payload.as_complex.real, what->payload.as_complex.imag); break;
        case PIK_TYPE_BOOL: asprintf(&buf, "%s", what->payload.as_int ? "true" : "false"); break;
        default:
            pik_object* string = pik_call_name(vm, what, "__str__", NULL, scope);
            if (vm->error) return NULL;
            if (string && string->type == PIK_TYPE_STRING) buf = strdup(string->payload.as_chars);
            else asprintf(&buf, "<%s at %p>", pik_typeof(vm, what), (void*)what);
            break;
    }
    return buf;
}

static pik_object* process_getvars(pik_vm* vm, pik_object* line, pik_object* self, pik_object* args, pik_object* scope) {
    IF_NULL_RETURN(vm) NULL;
    IF_NULL_RETURN(line) NULL;
    pik_object* new_line = create_codeobj(vm, line->flags.obj); // Same as original (list or line)
    for (size_t i = 0; i < line->payload.as_array.sz; i++) {
        pik_object* item = line->payload.as_array.items[i];
        if (item->type != PIK_TYPE_CODE || item->flags.obj != PIK_CODE_GETVAR) {
            pik_APPEND_INPLACE(new_line, item);
            continue;
        }
        pik_object* string = pik_create_primitive(vm, PIK_TYPE_STRING, (void*)item->payload.as_chars);
        pik_object* args = pik_create_primitive(vm, PIK_TYPE_LIST, NULL);
        pik_APPEND_INPLACE(args, string);
        pik_object* var = pik_call(vm, NULL, vm->dollar_function, args, scope);
        pik_decref(vm, string);
        pik_decref(vm, args);
        if (vm->error) {
            pik_decref(vm, new_line);
            return NULL;
        }
        // TODO: try get __getitem__ if list after
        pik_APPEND_INPLACE(new_line, var);
        pik_decref(vm, var);
    }
    return new_line;
}

static pik_object* eval_concat(pik_vm* vm, pik_object* concat, pik_object* self, pik_object* args, pik_object* scope) {
    IF_NULL_RETURN(vm) NULL;
    IF_NULL_RETURN(concat) NULL;
}

static pik_object* smoosh_expression(pik_vm* vm, pik_object* expr, pik_object* self, pik_object* args, pik_object* scope) {
    IF_NULL_RETURN(vm) NULL;
    IF_NULL_RETURN(expr) NULL;
    // todo: Find all unsupported operators and splice the strings together
}

static pik_object* eval_to_list(pik_vm* vm, pik_object* expr, pik_object* self, pik_object* args, pik_object* scope) {
    // TODO
}

static pik_object* eval_line(pik_vm* vm, pik_object* expr, pik_object* self, pik_object* args, pik_object* scope) {
    // TODO
}

static pik_object* eval_user_code(pik_vm* vm, pik_object* self, pik_object* scope, pik_object* code, pik_object* args) {
    IF_NULL_RETURN(vm) NULL;
    IF_NULL_RETURN(scope) NULL;
    IF_NULL_RETURN(code) NULL;
    // if (!self) self = scope;
    switch (code->type) {
        // todo: call stuff? break it down? return null or the object singleton?
    }
}

#ifdef PIK_TEST
static void dump_ast(pik_object* code, int indent) {
    if (code == NULL) {
        printf("NULL");
        return;
    }
    char* str;
    switch (code->type) {
        case PIK_TYPE_INT:
            printf("int(%lli)", code->payload.as_int);
            break;
        case PIK_TYPE_FLOAT:
            printf("float(%lg)", code->payload.as_double);
            break;
        case PIK_TYPE_COMPLEX:
            printf("complex(%g%+gj)", code->payload.as_complex.real, code->payload.as_complex.imag);
            break;
        case PIK_TYPE_BOOL:
            printf("bool(%s)", code->payload.as_int ? "true" : "false");
            break;
        case PIK_TYPE_STRING:
            printf("string(\"");
            str = code->payload.as_chars;
            for (size_t i = 0; i < strlen(str); i++) {
                if (needs_escape(str[i])) putchar('\\');
                putchar(escape(str[i]));
            }
            printf("\")");
            break;
        case PIK_TYPE_CODE:
            switch  (code->flags.obj) {
                case PIK_CODE_BLOCK:
                    printf("block(\n");
                    for (size_t i = 0; i < code->payload.as_array.sz; i++) {
                        if (i) printf(",\n");
                        printf("%*s", (indent + 1) * 4, "");
                        dump_ast(code->payload.as_array.items[i], indent + 1);
                    }
                    printf("\n%*s)", indent * 4, "");
                    break;
                case PIK_CODE_LINE:
                    printf("line(\n");
                    for (size_t i = 0; i < code->payload.as_array.sz; i++) {
                        if (i) printf(",\n");
                        printf("%*s", (indent + 1) * 4, "");
                        dump_ast(code->payload.as_array.items[i], indent + 1);
                    }
                    printf("\n%*s)", indent * 4, "");
                    break;
                case PIK_CODE_CONCAT:
                    printf("concat(\n");
                    for (size_t i = 0; i < code->payload.as_array.sz; i++) {
                        if (i) printf(",\n");
                        printf("%*s", (indent + 1) * 4, "");
                        dump_ast(code->payload.as_array.items[i], indent + 1);
                    }
                    printf("\n%*s)", indent * 4, "");
                    break;
                case PIK_CODE_GETVAR:
                case PIK_CODE_WORD:
                case PIK_CODE_OPERATOR:
                    printf("%s(%s)", code->flags.obj == PIK_CODE_GETVAR ? "getvar" : code->flags.obj == PIK_CODE_OPERATOR ? "operator" : "word", code->payload.as_chars);
                    break;
                case PIK_CODE_LIST:
                    printf("list(\n");
                    for (size_t i = 0; i < code->payload.as_array.sz; i++) {
                        if (i) printf(",\n");
                        printf("%*s", (indent + 1) * 4, "");
                        dump_ast(code->payload.as_array.items[i], indent + 1);
                    }
                    printf("\n%*s)", indent * 4, "");
                    break;
                default:
                    printf("Bad code type\n");
                    exit(70);
            }
            break;
        default:
            printf("<object type %u at %p>", code->type, (void*)code);
    }
}

void test_header(const char* h) {
    static int count = 0;
    count++;
    printf("\n-----------------%i: %s-----------------\n", count, h);
}

void dump_parser(pik_parser* p) {
    IF_NULL_RETURN(p);
    printf("%.*s\n", (int)(p->len - p->head), str_of(p));
}

int main(void) {
    pik_vm* vm = pik_new();
    test_header("Parser test");
    const char* code = 
        "# Test this\n"
        "def foobar [foo bar baz]:\n"
        "    print \"foo=\"$foo, \"bar=\"$bar, \"baz=\"$baz\n"
        "print \"hello world!\"\n"
        "print:\n"
        "                foobar\n"
        "                 barbaz\n"
        "if $x == $y:\n"
        "    print X equals Y!\n"
        "    while $y > 0:\n"
        "        print Y is going dooooown!!!!!!!!!!!!!!!!!!!\n"
        "        dec y\n"
        "print a list: [1 2 3 + 4]\n"
        "print a complex: 1+33j\n"
        "make x (123 + 456)\n"
        "$x |> $print ~> custom operator\n"
    ;
    pik_parser p = {.code = code, .len = strlen(code)};
    printf("Current code: ");
    dump_parser(&p);
    while (true) {
        if (p_eof(&p)) break;
        pik_object* res = compile_block(vm, &p);
        if (vm->error) break;
        printf("Dumping AST\n");
        dump_ast(res, 0);
        printf("\nFreeing AST\n");
        pik_decref(vm, res);
        dump_parser(&p);
        if (res == NULL) break;
    }
    if (vm->error) {
        printf("\nerror: %s\n", vm->error->payload.as_chars);
    }

    test_header("END tests");
    pik_destroy(vm);
    return 0;
}
#endif
    
#ifdef __cplusplus
}
#endif
