#include <stdlib.h>
#include <stdio.h>
#include <stdint.h>
#include <string.h>
#include <ctype.h>

#define PIK_DEBUG

#ifndef PIK_MAX_PARSER_DEPTH
#define PIK_MAX_PARSER_DEPTH 16384
#endif

#ifdef __cplusplus
extern "C" {
#endif

#pragma GCC optimize ("Os")

#if !defined(bool) && !defined(__cplusplus)
typedef uint8_t bool;
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

typedef pik_object* (*pik_builtin_function)(pik_vm*, pik_object*, size_t, pik_object**);

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
// TODO: scope

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
    bool locked;
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

typedef struct pik_parser pik_parser;
struct pik_parser {
    pik_parser* parent;
    const char* code;
    size_t len;
    size_t head;
    bool ieol;
    size_t depth;
};

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
    pik_parser* parser;
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
inline void pik_incref(pik_object*);
void pik_decref(pik_vm*, pik_object*);
static void mark_object(pik_vm*, pik_object*);
static inline pik_hashmap* new_hashmap(void);
static void hashmap_destroy(pik_vm*, pik_hashmap*);
static pik_object* next_item(pik_vm*, pik_parser*);

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
    PIK_DEBUG_PRINTF("Allocating new memory");
    object = (pik_object*)calloc(1, sizeof(pik_object));
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
    while (vm->parser) {
        pik_parser* p = vm->parser;
        vm->parser = p->parent;
        PIK_DEBUG_PRINTF("Freeing a parser with code \"%.*s%s\"\n", (int)(p->len < 15 ? p->len : 15), p->code, p->len < 15 ? "" : "...");
        free(p);
    }
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
        if (streq(b->entries[i].key, key)) return &b->entries[i];
    }
    return NULL;
}

void pik_hashmap_put(pik_vm* vm, pik_hashmap* map, const char* key, pik_object* value, bool locked) {
    PIK_DEBUG_PRINTF("Adding %s at %p to hashmap at key \"%s\"%s\n", pik_typeof(vm, value), (void*)value, key, locked ? " (locked)" : "");
    IF_NULL_RETURN(map);
    pik_incref(value);
    pik_hashbucket* b = get_bucket(map, key);
    pik_hashentry* e = find_entry(b, key);
    if (e) {
        PIK_DEBUG_PRINTF("used old entry for %s\n", key);
        pik_decref(vm, e->value);
        e->value = value;
        e->locked = locked;
        return;
    }
    PIK_DEBUG_PRINTF("new entry for %s\n", key);
    b->entries = (pik_hashentry*)realloc(b->entries, sizeof(pik_hashentry) * (b->sz + 1));
    b->entries[b->sz].key = strdup(key);
    b->entries[b->sz].value = value;
    b->entries[b->sz].locked = locked;
    b->sz++;
}

pik_object* pik_hashmap_get(pik_hashmap* map, const char* key) {
    PIK_DEBUG_PRINTF("Getting entry %s on hashmap %p\n", key, (void*)map);
    IF_NULL_RETURN(map) NULL;
    pik_hashentry* e = find_entry(get_bucket(map, key), key);
    if (e) {
        PIK_DEBUG_PRINTF("key %s found\n", key);
        pik_incref(e->value);
        return e->value;
    }
    PIK_DEBUG_PRINTF("key %s not found\n", key);
    return NULL;
}

bool pik_hashmap_entry_is_locked(pik_hashmap* map, const char* key) {
    PIK_DEBUG_PRINTF("Getting lock bit of %s on hashmap %p\n", key, (void*)map);
    IF_NULL_RETURN(map) false;
    pik_hashentry* e = find_entry(get_bucket(map, key), key);
    if (e) {
        PIK_DEBUG_PRINTF("key %s found: %s\n", key, e->locked ? "locked" : "writable");
        return e->locked;
    }
    PIK_DEBUG_PRINTF("key %s not found\n", key);
    return false;
}

bool pik_hashmap_has(pik_hashmap* map, const char* key) {
    PIK_DEBUG_PRINTF("Testing presence of key: ");
    return pik_hashmap_get(map, key) != NULL;
}

// ------------------------------------- Builtin primitive types ----------------------------------

void pik_tf_STRDUP_ARG(pik_vm* vm, pik_object* object, void* arg) {
    (void)vm;
    object->payload.as_pointer = (void*)strdup((char*)arg);
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

void pik_tf_DECREF_IF_USER_CODE(pik_vm* vm, pik_object* object, void* arg) {
    (void)arg;
    if (object->flags.obj & PIK_FUNCTION_FLAG_IS_USER) {
        pik_decref(vm, object->payload.as_object);
        object->payload.as_object = NULL;
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
    pik_register_type(vm, PIK_TYPE_FUNCTION, "function", pik_tf_COPY_PTR, pik_tf_NOOP, pik_tf_DECREF_IF_USER_CODE);
    pik_register_type(vm, PIK_TYPE_CLASS, "class", pik_tf_COPY_PTR, pik_tf_NOOP, pik_tf_DECREF_IF_USER_CODE);
    pik_register_type(vm, PIK_TYPE_CODE, "code", pik_tf_init_code, pik_tf_mark_code, pik_tf_free_code);
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

static inline void advance(pik_parser* p, ssize_t delta) {
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

static inline bool eolchar(char c, bool ieol) {
    if (c == ';') return true;
    if (ieol) return false;
    return c == '\n' || c == '\r';
}

static inline bool p_endline(pik_parser* p) {
    IF_NULL_RETURN(p) true;
    if (p_eof(p)) return true;
    return eolchar(at(p), false);
}

static inline bool p_startswith(pik_parser* p, const char* str) {
    return strncmp(str_of(p), str, strlen(str)) == 0;
}

static pik_parser* push_parser(pik_vm* vm, pik_parser* p, const char* str, size_t len, bool ieol) {
    IF_NULL_RETURN(vm) NULL;
    if (p && p->depth + 1 > PIK_MAX_PARSER_DEPTH) {
        pik_set_error(vm, "too much recursion");
        return NULL;
    }
    pik_parser* next = (pik_parser*)calloc(1, sizeof(pik_parser));
    next->parent = p;
    next->code = str;
    next->len = len > 0 ? len : strlen(str);
    next->ieol = ieol;
    if (p) next->depth = p->depth + 1;
    return next;
}

#ifdef PIK_DEBUG
void PIK_DEBUG_DUMP_PARSER(pik_parser* p) {
    IF_NULL_RETURN(p);
    printf("\"%.*s\"\n", (int)(p->len - p->head), str_of(p));
}
#endif

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
    return strchr("{}\b\t\n\v\f\r\a\\", c) != NULL;
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

static bool valid_opchr(char c) {
    return strchr("`~!@#%^&*_-+=<>,./|:;", c) != NULL;
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
        } else if (c == '\\' && eolchar(peek(p, 1), p->ieol)) {
            // Escaped EOL
            next(p);
            while (!p_endline(p)) next(p);
        } else if (eolchar(c, p->ieol)) {
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
    asprintf((char**)&gv->payload.as_pointer, "%.*s", (int)len, str_of(p));
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
    next(p);
    pik_object* expr = create_codeobj(vm, PIK_CODE_LINE);
    while (true) {
        if (at(p) == ')') {
            next(p); // Skip over )
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
    PIK_DEBUG_PRINTF("UNIMPLEMENTED\n");
    return NULL;
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
    while (!isspace(at(p)) && !p_eof(p) && !(valid_opchr(at(p)) ^ is_operator)) {
        len++;
        next(p);
    }
    // Pick it out
    size_t end = save(p);
    restore(p, start);
    pik_object* w = create_codeobj(vm, ispunct(at(p)) ? PIK_CODE_OPERATOR : PIK_CODE_WORD);
    asprintf((char**)&w->payload.as_pointer, "%.*s", (int)len, str_of(p));
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
        default:   if (eolchar(at(p), p->ieol)) return result; else next = get_word(vm, p); break;
    }
    if (!vm->error && save(p) == here) {
        // Generic failed to parse message
        pik_set_error_fmt(vm, "syntax error: failed to parse: %.20s...", str_of(p));
    }
    if (vm->error) return NULL;
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
            if (vm->error) return NULL;
            if (eolchar(at(p), p->ieol)) {
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

#ifdef PIK_DEBUG
static void dump_ast(pik_object* code, int indent) {
    if (code == NULL) {
        printf("NULL");
        return;
    }
    char* str;
    switch (code->type) {
        case PIK_TYPE_INT:
            printf("#int(%lli)", code->payload.as_int);
            break;
        case PIK_TYPE_FLOAT:
            printf("#float(%lg)", code->payload.as_double);
            break;
        case PIK_TYPE_COMPLEX:
            printf("#complex(%g%+gj)", code->payload.as_complex.real, code->payload.as_complex.imag);
            break;
        case PIK_TYPE_BOOL:
            printf("#bool(%s)", code->payload.as_int ? "true" : "false");
            break;
        case PIK_TYPE_STRING:
            printf("#string(\"");
            str = (char*)code->payload.as_pointer;
            for (size_t i = 0; i < strlen(str); i++) {
                if (needs_escape(str[i])) putchar('\\');
                putchar(escape(str[i]));
            }
            printf("\")");
            break;
        case PIK_TYPE_CODE:
            switch  (code->flags.obj) {
                case PIK_CODE_BLOCK:
                    printf("#block(\n");
                    for (size_t i = 0; i < code->payload.as_array.sz; i++) {
                        if (i) printf(",\n");
                        printf("%*s", (indent + 1) * 4, "");
                        dump_ast(code->payload.as_array.items[i], indent + 1);
                    }
                    printf("\n%*s)", indent * 4, "");
                    break;
                case PIK_CODE_LINE:
                    printf("#line(\n");
                    for (size_t i = 0; i < code->payload.as_array.sz; i++) {
                        if (i) printf(",\n");
                        printf("%*s", (indent + 1) * 4, "");
                        dump_ast(code->payload.as_array.items[i], indent + 1);
                    }
                    printf("\n%*s)", indent * 4, "");
                    break;
                case PIK_CODE_CONCAT:
                    printf("#concat(\n");
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
                    printf("#%s(%s)", code->flags.obj == PIK_CODE_GETVAR ? "getvar" : code->flags.obj == PIK_CODE_OPERATOR ? "operator" : "word", (char*)code->payload.as_pointer);
                    break;
                case PIK_CODE_LIST:
                    printf("#list_todo");
                    break;
                default:
                    PIK_DEBUG_ASSERT(false, "Bad code type");
                    break;
            }
            break;
        default:
            printf("<object type %u at %p>", code->type, (void*)code);
    }
}
#endif

#ifdef PIK_DEBUG
void test_header(const char* h) {
    static int count = 0;
    count++;
    printf("\n-----------------%i: %s-----------------\n", count, h);
}

int main(void) {
    pik_vm* vm = pik_new();
    // test_header("Create 10 garbage objects -- should reuse");
    // size_t init = vm->gc.num_objects;
    // for (size_t i = 0; i < 10; i++) {
    //     pik_decref(vm, alloc_object(vm, PIK_TYPE_NONE, NULL));
    // }
    // size_t created = vm->gc.num_objects - init;
    // PIK_DEBUG_ASSERT(created == 1, "failed to reuse object with no refs");

    // test_header("Create non garbage");
    // char* buf;
    // pik_object* top = alloc_object(vm, PIK_TYPE_NONE, NULL);
    // for (size_t i = 0; i < 5; i++) {
    //     unsigned int h = hashmap_hash(buf);
    //     free(buf);
    //     buf = NULL;
    //     asprintf(&buf, "MyProp%u", h + 1);
    //     pik_object* obj = alloc_object(vm, PIK_TYPE_NONE, NULL);
    //     pik_hashmap_put(vm, top->properties, buf, obj, true);
    //     pik_decref(vm, obj);
    // }
    // free(buf);
    // test_header("Check hashmap_has()");
    // PIK_DEBUG_ASSERT(pik_hashmap_has(top->properties, "MyProp6"), "pik_hashmap_has() doesn't work");
    // test_header("Check hashmap_is_locked()");
    // PIK_DEBUG_ASSERT(pik_hashmap_entry_is_locked(top->properties, "MyProp6"), "entry->locked wasn't set right");

    // test_header("triggering tombstoning of all");
    // pik_decref(vm, top);
    // pik_collect_garbage(vm);

    test_header("Parser test");
    const char* code = R"===(

# Test this
print "hello world!"
print:
                foobar
                barbaz
make x (123 + 456)
$x |> $print

)===";
    pik_parser* p = push_parser(vm, NULL, code, 0, false);
    PIK_DEBUG_ASSERT(p != NULL, "Failed to push parser");
    PIK_DEBUG_ASSERT(p->depth == 0, "Set incorrect depth on parser");
    PIK_DEBUG_DUMP_PARSER(p);
    while (1) {
        pik_object* res = compile_block(vm, p);
        printf("Dumping AST\n");
        dump_ast(res, 0);
        printf("\nFreeing AST\n");
        pik_decref(vm, res);
        PIK_DEBUG_DUMP_PARSER(p);
        if (res == NULL) break;
    }
    if (vm->error) {
        PIK_DEBUG_ASSERT(false, (const char*)vm->error->payload.as_pointer);
    }

    test_header("END tests");
    pik_destroy(vm);
    return 0;
}
#endif
    
#ifdef __cplusplus
}
#endif
