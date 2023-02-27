#include <stdlib.h>
#include <stdio.h>
#include <stdint.h>
#include <string.h>
#include <assert.h>

#define PIK_DEBUG

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
#else
#define PIK_DEBUG_PRINTF(...)
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

#define streq(a, b) (!strcmp(a, b))

// Types
#define PIK_TYPE_TOMBSTONE UINT16_MAX
#define PIK_TYPE_NONE 0

// Flags
#define PIK_MARKED 1

#define PIK_HASHMAP_BUCKETS 256
#define PIK_HASHMAP_BUCKETMASK 0xFF
typedef struct pik_hashentry {
    char* key;
    pik_object* value;
    bool readonly;
} pik_hashentry;

typedef struct pik_hashbucket {
    pik_hashentry* entries;
    size_t sz;
} pik_hashbucket;

typedef struct pik_hashmap {
    pik_hashbucket buckets[PIK_HASHMAP_BUCKETS];
} pik_hashmap;

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
        struct {
            float real;
            float imag;
        } as_complex;
    } payload;
};

typedef struct pik_typemgr {
    pik_type_function funs[PIK_NUMTYPEFUNS];
    pik_type type_for;
    const char* type_string;
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
    bool ignore_eol;
    size_t depth;
};

struct pik_vm {
    struct {
        pik_object* first;
        pik_object* tombstones;
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
if (vm->callbacks.name != NULL) { \
    vm->callbacks.name(vm, __VA_ARGS__);\
}

// Forward references
void pik_register_globals(pik_vm*);
inline void pik_incref(pik_object*);
void pik_decref(pik_vm*, pik_object*);
inline pik_hashmap* pik_hashmap_new(void);
void pik_hashmap_destroy(pik_vm*, pik_hashmap*);

// ------------------- Alloc/dealloc objects -------------------------

const char* pik_typename(pik_vm* vm, pik_type type) {
    for (size_t i = 0; i < vm->type_managers.sz; i++) { 
        if (vm->type_managers.mgrs[i].type_for == type) { 
            return vm->type_managers.mgrs[i].type_string;
        }
    }
    return "object";
}

const char* pik_typeof(pik_vm* vm, pik_object* object) {
    if (object == NULL) return "null";
    return pik_typename(vm, object->type);
}

void pik_run_typefun(pik_vm* vm, pik_type type, pik_object* object, void* arg, int name) {
    for (size_t i = 0; i < vm->type_managers.sz; i++) { 
        if (vm->type_managers.mgrs[i].type_for == type && vm->type_managers.mgrs[i].funs[name] != NULL) { 
            vm->type_managers.mgrs[i].funs[name](vm, object, arg); 
            return; 
        } 
    }
}

pik_object* pik_alloc_object(pik_vm* vm, pik_type type, void* arg) {
    pik_object* object;
    PIK_DEBUG_PRINTF("Allocating object %s: ", pik_typename(vm, type));
    if (vm->gc.tombstones == NULL) {
        PIK_DEBUG_PRINTF("fresh calloc()\n");
        object = (pik_object*)calloc(1, sizeof(struct pik_object));
        object->gc.next = vm->gc.first;
        vm->gc.first = object;
        vm->gc.num_objects++;
    } else {
        PIK_DEBUG_PRINTF("using a tombstone");
        object = vm->gc.tombstones;
        vm->gc.tombstones = (pik_object*)object->payload.as_pointer;
        PIK_DEBUG_PRINTF("%s\n", vm->gc.tombstones == NULL ? " (no more left)" : "");
    }
    object->type = type;
    object->gc.refcnt = 1;
    object->properties = pik_hashmap_new();
    pik_run_typefun(vm, type, object, arg, PIK_INITFUN);
    return object;
}

void pik_add_prototype(pik_object* object, pik_object* proto) {
    if (object == NULL) return;
    if (proto == NULL) return;
    for (size_t i = 0; i < object->proto.sz; i++) {
        if (object->proto.bases[i] == proto) return; // Don't add the same prototype twice
    }
    object->proto.bases = (pik_object**)realloc(object->proto.bases, (object->proto.sz + 1) * sizeof(struct pik_object));
    object->proto.bases[object->proto.sz] = proto;
    object->proto.sz++;
    pik_incref(proto);
}

pik_object* pik_create_fromproto(pik_vm* vm, pik_type type, pik_object* proto, void* arg) {
    pik_object* object = pik_alloc_object(vm, type, arg);
    pik_add_prototype(object, proto);
    return object;
}

inline void pik_incref(pik_object* object) {
    if (object == NULL) return;
    object->gc.refcnt++;
    PIK_DEBUG_PRINTF("object %p got a new reference (now have %zu)\n", (void*)object, object->gc.refcnt);
}

void pik_finalize(pik_vm* vm, pik_object* object) {
    if (object == NULL) return;
    if (object->type == PIK_TYPE_TOMBSTONE) {
        PIK_DEBUG_PRINTF("Finalize tombstone at %p (noop)\n", (void*)object);
        return;
    }
    PIK_DEBUG_PRINTF("Finalizing %s object at %p\n", pik_typeof(vm, object), (void*)object);
    pik_type type = object->type;
    object->type = PIK_TYPE_TOMBSTONE;
    // Free object-specific stuff
    pik_run_typefun(vm, type, object, NULL, PIK_FINALFUN);
    assert(object->payload.as_int == 0);
    // Free everything else
    object->flags.global = 0;
    object->flags.obj = 0;
    for (size_t i = 0; i < object->proto.sz; i++) {
        pik_decref(vm, object->proto.bases[i]);
    }
    free(object->proto.bases);
    object->proto.bases = NULL;
    object->proto.sz = 0;
    pik_hashmap_destroy(vm, object->properties);
    object->properties = NULL;
}

void pik_decref(pik_vm* vm, pik_object* object) {
    if (object == NULL) return;
    object->gc.refcnt--;
    if (object->gc.refcnt == 0) {
        PIK_DEBUG_PRINTF("object %s at %p lost all references, making a tombstone\n", pik_typeof(vm, object), (void*)object);
        // Free it now, no other references
        pik_finalize(vm, object);
        // Put it in the tombstone list
        object->payload.as_pointer = (void*)vm->gc.tombstones;
        vm->gc.tombstones = object;
        // Unmark it so it will be collected if a GC is ongoing
        object->flags.global &= ~PIK_MARKED;
    }
    else {
        PIK_DEBUG_PRINTF("object %s at %p lost a reference (now have %zu)\n", pik_typeof(vm, object), (void*)object, object->gc.refcnt);
    }
}

void pik_mark_object(pik_vm* vm, pik_object* object) {
    mark:
    PIK_DEBUG_PRINTF("Marking %s at %p:\n", pik_typeof(vm, object), (void*)object);
    if (object == NULL || object->flags.global & PIK_MARKED) return;
    object->flags.global |= PIK_MARKED;
    // Mark payload
    PIK_DEBUG_PRINTF("%p->payload\n", (void*)object);
    pik_run_typefun(vm, object->type, object, NULL, PIK_MARKFUN);
    size_t i, j;
    // Mark properties
    PIK_DEBUG_PRINTF("%p->properties\n", (void*)object);
    for (i = 0; i < PIK_HASHMAP_BUCKETS; i++) {
        pik_hashbucket b = object->properties->buckets[i];
        #ifdef PIK_DEBUG
        if (b.sz > 0) printf("%p->properties->buckets[%zu]\n", (void*)object, i);
        #endif
        for (j = 0; j < b.sz; j++) {
            pik_mark_object(vm, b.entries[i].value);
        }
    }
    if (object->proto.sz == 0) {
        PIK_DEBUG_PRINTF("%p->prototype == null\n", (void*)object);
        return;
    }
    // Mark prototypes
    // - 1 for tail-call optimization
    PIK_DEBUG_PRINTF("%p->prototype (%zu bases)\n", (void*)object, object->proto.sz);
    for (i = 0; i < object->proto.sz - 1; i++) {
        pik_mark_object(vm, object->proto.bases[i]);
    }
    // Tail-call optimize
    object = object->proto.bases[i]; // i is now == object->proto.sz - 1
    goto mark;
}

size_t pik_dogc(pik_vm* vm) {
    if (vm == NULL) return 0;
    PIK_DEBUG_PRINTF("Entering garbage colletion\n");
    size_t freed = 0;
    // Sweep the tombstones first, they form a linked list we don't want broken
    while (vm->gc.tombstones != NULL) {
        PIK_DEBUG_PRINTF("Freeing tombstone at %p\n", (void*)vm->gc.tombstones);
        pik_object* tombstone = vm->gc.tombstones;
        vm->gc.tombstones = (pik_object*)tombstone->payload.as_pointer;
        // No need to finalize; it has already been done
        free(tombstone);
        freed++;
    }
    // Mark everything else reachable 
    pik_mark_object(vm, vm->global_scope);
    pik_mark_object(vm, vm->dollar_function);
    // Sweep 
    pik_object** object = &vm->gc.first;
    while (*object != NULL) {
        PIK_DEBUG_PRINTF("Looking at object %s at %p: ", pik_typeof(vm, *object), (void*)(*object));
        if ((*object)->flags.global & PIK_MARKED) {
            PIK_DEBUG_PRINTF("unmarked\n");
            // Sweep the object
            pik_object* unreached = *object;
            *object = unreached->gc.next;
            pik_finalize(vm, unreached);
            free(unreached);
            vm->gc.num_objects--;
            freed++;
        } else {
            PIK_DEBUG_PRINTF("marked\n");
            // Keep the object
            (*object)->flags.global &= ~PIK_MARKED;
            assert(!((*object)->flags.global & PIK_MARKED));
            object = &(*object)->gc.next;
        }
    }
    PIK_DEBUG_PRINTF("%zu freed, %zu objects remaining after gc\n", freed, vm->gc.num_objects);
    return freed;
}

pik_vm* pik_new(void) {
    pik_vm* vm = (pik_vm*)calloc(1, sizeof(struct pik_vm));
    vm->global_scope = pik_alloc_object(vm, PIK_TYPE_NONE, NULL);
    // TODO: register global functions
    return vm;
}

void pik_destroy(pik_vm* vm) {
    if (vm == NULL) return;
    PIK_DEBUG_PRINTF("Freeing the VM - garbage collect all: ");
    vm->global_scope = NULL;
    vm->dollar_function = NULL;
    pik_dogc(vm);
    while (vm->gc.first != NULL) {
        pik_object* object = vm->gc.first;
        vm->gc.first = (pik_object*)object->gc.next;
        pik_finalize(vm, object);
        free(object);
    }
    free(vm->type_managers.mgrs);
    free(vm->operators.ops);
    free(vm);
}

// ------------------------- Hashmap stuff -------------------------

inline pik_hashmap* pik_hashmap_new(void) {
    return (pik_hashmap*)calloc(1, sizeof(struct pik_hashmap));
}

unsigned int pik_hashmap_hash(const char* value) {
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

void pik_hashmap_destroy(pik_vm* vm, pik_hashmap* map) {
    if (map == NULL) return;
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

void pik_hashmap_put(pik_vm* vm, pik_hashmap* map, const char* key, pik_object* value, bool readonly) {
    PIK_DEBUG_PRINTF("Adding object %s at %p to hashmap at key \"%s\"%s\n", pik_typeof(vm, value), (void*)value, key, readonly ? " (readonly)" : "");
    unsigned int hash = pik_hashmap_hash(key);
    pik_hashbucket* b = &map->buckets[hash];
    pik_incref(value);
    PIK_DEBUG_PRINTF("Checking %zu existing entries in bucket %u:\n", b->sz, hash);
    for (size_t i = 0; i < b->sz; i++) {
        if (streq(b->entries[i].key, key)) {
            PIK_DEBUG_PRINTF("used old entry for %s\n", key);
            pik_decref(vm, b->entries[i].value);
            b->entries[i].value = value;
            b->entries[i].readonly = readonly;
            return;
        }
    }
    PIK_DEBUG_PRINTF("new entry for %s\n", key);
    b->entries = (pik_hashentry*)realloc(b->entries, sizeof(struct pik_hashentry) * (b->sz + 1));
    b->entries[b->sz].key = strdup(key);
    b->entries[b->sz].value = value;
    b->sz++;
}

#ifdef PIK_DEBUG
int main(void) {
    pik_vm* vm = pik_new();
    pik_object* object = pik_alloc_object(vm, PIK_TYPE_NONE, NULL);
    pik_decref(vm, object);
    object = pik_alloc_object(vm, PIK_TYPE_NONE, NULL);
    printf("Create garbage\n");
    for (size_t i = 0; i < 10; i++) {
        pik_alloc_object(vm, PIK_TYPE_NONE, NULL);
    }
    printf("Create non garbage\n");
    for (size_t i = 0; i < 10; i++) {
        pik_object* obj = pik_alloc_object(vm, PIK_TYPE_NONE, NULL);
        pik_hashmap_put(vm, object->properties, "MyProperty", obj, true);
    }
    puts("triggering tombstoning of all\n");
    pik_decref(vm, object);
    pik_dogc(vm);
    pik_destroy(vm);
    return 0;
}
#endif
    
#ifdef __cplusplus
}
#endif
