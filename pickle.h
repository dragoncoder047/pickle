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

typedef uint16_t pik_type;
typedef uint16_t pik_flags;
typedef uint16_t pik_resultcode;
typedef struct pik_object pik_object;
typedef struct pik_vm pik_vm;

typedef pik_object* (*pik_builtin_function)(pik_vm*, pik_object*, size_t, pik_object**);

// These function only operate on the void* payload of the object, everything else is handles automatically
typedef void (*pik_type_function)(pik_vm*, pik_object*);
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

#define PIK_TYPE_TOMBSTONE UINT16_MAX
#define PIK_TYPE_NONE 0

#define PIK_HASHMAP_BUCKETS 256
#define PIK_HASHMAP_BUCKETMASK 0xFF
typedef struct pik_hashentry {
    char* key;
    pik_object* value;
    bool readonly;
} pik_hashentry;

typedef struct pik_hashbucket {
    pik_hashentry* entries;
    size_t cap;
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
        size_t cap;
        size_t len;
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
        size_t len;
        size_t cap;
    } type_managers;
    struct {
        pik_operator* ops;
        size_t len;
        size_t cap;
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
    vm->callbacks.name(__VA_ARGS__);\
}

// Forward references
void pik_register_globals(pik_vm*);
void pik_hashmap_destroy(pik_vm*, pik_hashmap*);
void pik_decref(pik_vm*, pik_object*);
inline pik_hashmap* pik_hashmap_new(void);

// ------------------- Alloc/dealloc objects -------------------------
    
void pik_run_typefun(pik_vm* vm, pik_type type, pik_object* object, int name) {
    for (size_t i = 0; i < vm->type_managers.len; i++) { 
        if (vm->type_managers.mgrs[i].type_for == type && vm->type_managers.mgrs[i].funs[name] != NULL) { 
            vm->type_managers.mgrs[i].funs[name](vm, object); 
            return; 
        } 
    }
}

pik_object* pik_alloc_object(pik_vm* vm, pik_type type) {
    pik_object* object;
    if (vm->gc.tombstones == NULL) {
        object = (pik_object*)calloc(1, sizeof(struct pik_object));
        object->gc.next = vm->gc.first;
        vm->gc.first = object;
        vm->gc.num_objects++;
    } else {
        object = vm->gc.tombstones;
        vm->gc.tombstones = (pik_object*)object->payload.as_pointer;
    }
    object->type = type;
    object->gc.refcnt = 1;
    object->properties = pik_hashmap_new();
    pik_run_typefun(vm, type, object, PIK_INITFUN);
    return object;
}

void pik_finalize(pik_vm* vm, pik_object* object) {
    pik_type type = object->type;
    object->type = PIK_TYPE_TOMBSTONE;
    // Free object-specific stuff
    pik_run_typefun(vm, type, object, PIK_FINALFUN);
    // Free everything else
    object->flags.global = 0;
    object->flags.obj = 0;
    for (size_t i = 0; i < object->proto.len; i++) {
        pik_decref(vm, object->proto.bases[i]);
    }
    free(object->proto.bases);
    object->proto.bases = NULL;
    object->proto.len = 0;
    object->proto.cap = 0;
    pik_hashmap_destroy(vm, object->properties);
    object->properties = NULL;
}

#define pik_incref(object) ((object)->gc.refcnt++)

void pik_decref(pik_vm* vm, pik_object* object) {
    if (object == NULL) return;
    object->gc.refcnt--;
    if (object->gc.refcnt == 0) {
        // Free it now, no other references
        pik_finalize(vm, object);
        // Put it in the tombstone list
        object->payload.as_pointer = (void*)vm->gc.tombstones;
        vm->gc.tombstones = object;
    }
}

pik_vm* pik_new(void) {
    pik_vm* vm = (pik_vm*)calloc(1, sizeof(struct pik_vm));
    vm->global_scope = pik_alloc_object(vm, PIK_TYPE_NONE);
    return vm;
}

// ------------------------- Hashmap stuff -------------------------

inline pik_hashmap* pik_hashmap_new(void) {
    return (pik_hashmap*)calloc(1, sizeof(struct pik_hashmap));
}

unsigned int pik_hashmap_hash(const char* value) {
    unsigned int hash = 5381;
    for (char c = *value; c; c = *value++) {
        hash = (hash << 5) + hash + c;
    }
    return hash & PIK_HASHMAP_BUCKETMASK;
}
    
void pik_hashmap_destroy(pik_vm* vm, pik_hashmap* map) {
    if (map == NULL) return;
    for (size_t b = 0; b < PIK_HASHMAP_BUCKETS; b++) {
        pik_hashbucket bucket = map->buckets[b];
        for (size_t e = 0; e < bucket.cap; e++) {
            free(bucket.entries[e].key);
            pik_decref(vm, bucket.entries[e].value);
        }
    }
    free(map);
}
    
void pik_hashmap_put(pik_hashmap* map, const char* key, pik_object* value, bool readonly) {
    pik_incref(value);
    pik_hashbucket b = map->buckets[pik_hashmap_hash(key)];
    for (size_t i = 0; i < b.cap; i++) {
        if (streq(b.entries[i].key, key)) {
            b.entries[i].value = value;
            b.entries[i].readonly = readonly;
            return;
        }
    }
    b.entries = (pik_hashentry*)realloc(b.entries, sizeof(struct pik_hashentry) * (b.cap + 1));
    b.entries[b.cap].key = strdup(key);
    b.entries[b.cap].value = value;
    b.cap++;
}

#ifdef PIK_DEBUG
int main(void) {
    printf("%lu %lu %lu\nfoobarbaz", sizeof(struct pik_object), sizeof(struct pik_vm), sizeof(struct pik_hashmap));
    return 0;
}
#endif
    
#ifdef __cplusplus
}
#endif
