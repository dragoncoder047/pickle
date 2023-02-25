#include <stdlib.h>
#include <stdio.h>
#include <stdint.h>
#include <string.h>
#include <assert.h>

#define DEBUG

#ifdef __cplusplus
extern "C" {
#endif

#pragma GCC optimize ("Os")

#if !defined(bool) && !defined(__cplusplus)
typedef uint8_t bool;
#define true 1
#define false 0
#endif

typedef uint16_t pk_type;
typedef uint16_t pk_flags;
typedef uint16_t pk_resultcode;
typedef struct pk_object pk_object;
typedef struct pk_vm pk_vm;

typedef pk_object* (*pk_builtin_function)(pk_vm*, pk_object*, size_t, pk_object**);

// These function only operate on the void* payload of the object, everything else is handles automatically
typedef void (*pk_type_function_t)(pk_vm*, pk_object*);
#define PK_NUMTYPEFUNS 3
#define PK_INITFUN 0
#define PK_MARKFUN 1
#define PK_FINALFUN 2

typedef void (*pk_exit_callback)(pk_vm*, pk_object*);
typedef void (*pk_write_callback)(pk_vm*, const char*);
typedef char* (*pk_read_callback)(pk_vm*, const char*);
typedef char* (*pk_source_callback)(pk_vm*, const char*);
typedef void (*pk_store_callback)(pk_vm*, const char*, const char*);
typedef void (*pk_error_callback)(pk_vm*, size_t, const char*);
typedef const char* (*pk_embeddedfilter_callback)(pk_vm*, const char*);
typedef const char* (*pk_checkinterrupt_callback)(pk_vm*);

#define streq(a, b) (!strcmp(a, b))

#define PK_TYPE_TOMBSTONE UINT16_MAX
#define PK_TYPE_NONE 0

#define PK_HASHMAP_BUCKETS 256
#define PK_HASHMAP_BUCKETMASK 0xFF
typedef struct pk_hashentry {
    char* key;
    pk_object* value;
    bool readonly;
} pk_hashentry;

typedef struct pk_hashbucket {
    pk_hashentry* entries;
    size_t cap;
} pk_hashbucket;

typedef struct pk_hashmap {
    pk_hashbucket buckets[PK_HASHMAP_BUCKETS];
} pk_hashmap;

struct pk_object {
    pk_type type;
    struct {
        pk_object* next;
        size_t refcnt;
    } gc;
    struct {
        pk_flags global;
        pk_flags obj;
    } flags;
    struct {
        pk_object** bases;
        size_t cap;
        size_t len;
    } proto;
    pk_hashmap* properties;
    void* payload;
//     union {
//         int64_t as_int;
//         double as_double;
//         bool as_bool;
//         struct {
//             float real;
//             float imag;
//         } as_complex;
//         struct {
//             pk_object** items;
//             size_t len;
//             size_t cap;
//         } as_list;
//         pk_hashmap* as_hashmap;
//         union {
//             pk_builtin_function c_func;
//             struct {
//                 const char** argnames;
//                 size_t argc;
//                 pk_object* closure;
//             } user;
//         } as_func;
//         const char* as_string;
//         struct {
//             const char* message;
//             // Leave room for tracebacks, if I ever implement that
//         } as_error;
//         struct {
//             pk_object* result;
//             pk_resultcode resultcode;
//         } as_scope;
//         struct {
//             pk_object** code;
//             size_t len;
//             size_t cap;
//         } as_code;
//         struct {
//             pk_object* car;
//             pk_object* cdr;
//         } as_cons;
//     } payload;
};

typedef struct pk_typemgr {
    pk_type_function_t funs[PK_NUMTYPEFUNS];
    pk_type type_for;
    const char* type_string;
} pk_typemgr;

typedef struct pk_operator {
    const char* symbol;
    const char* method;
    int precedence;
} pk_operator;

typedef struct pk_parser pk_parser;
struct pk_parser {
    pk_parser* parent;
    const char* code;
    size_t len;
    size_t head;
    bool ignore_eol;
    size_t depth;
};

struct pk_vm {
    struct {
        pk_object* first;
        pk_object* tombstones;
    } gc;
    struct {
        pk_typemgr* mgrs;
        size_t len;
        size_t cap;
    } type_managers;
    struct {
        pk_operator* ops;
        size_t len;
        size_t cap;
    } operators;
    pk_parser* parser;
    pk_object* global_scope;
    pk_object* dollar_function;
    struct {
        pk_exit_callback exit;
        pk_write_callback write;
        pk_read_callback read;
        pk_source_callback source;
        pk_store_callback store;
        pk_error_callback error;
        pk_embeddedfilter_callback embeddedfilter;
        pk_checkinterrupt_callback checkinterrupt;
    } callbacks;
};

#define PK_DOCALLBACK(vm, name, ...) \
if (vm->callbacks.name != NULL) { \
    vm->callbacks.name(__VA_ARGS__);\
}

// Forward references
void pk_register_globals(pk_vm*);
void pk_hashmap_destroy(pk_vm*, pk_hashmap*);
void pk_decref(pk_vm*, pk_object*);

// ------------------- Alloc/dealloc objects -------------------------
    
void pk_run_typefun(pk_vm* vm, pk_type type, pk_object* object, int name) {
    for (size_t i = 0; i < vm->type_managers.len; i++) { 
        if (vm->type_managers.mgrs[i].type_for == type && vm->type_managers.mgrs[i].funs[name] != NULL) { 
            vm->type_managers.mgrs[i].funs[name](vm, object); 
            return; 
        } 
    }
}

pk_object* pk_alloc_object(pk_vm* vm, pk_type type) {
    pk_object* object;
    if (vm->gc.tombstones == NULL) {
        object = (pk_object*)calloc(1, sizeof(struct pk_object));
        object->gc.next = vm->gc.first;
        vm->gc.first = object;
    } else {
        object = vm->gc.tombstones;
        vm->gc.tombstones = (pk_object*)object->payload;
    }
    object->type = type;
    object->gc.refcnt = 1;
    pk_run_typefun(vm, type, object, PK_INITFUN);
    return object;
}
    

void pk_finalize(pk_vm* vm, pk_object* object) {
    pk_type type = object->type;
    object->type = PK_TYPE_TOMBSTONE;
    // Free object-specific stuff
    pk_run_typefun(vm, type, object, PK_FINALFUN);
    // Free everything else
    object->flags.global = 0;
    object->flags.obj = 0;
    for (size_t i = 0; i < object->proto.len; i++) {
        pk_decref(vm, object->proto.bases[i]);
    }
    free(object->proto.bases);
    object->proto.bases = NULL;
    object->proto.len = 0;
    object->proto.cap = 0;
    pk_hashmap_destroy(vm, object->properties);
    object->properties = NULL;
}

#define pk_incref(object) ((object)->gc.refcnt++)

void pk_decref(pk_vm* vm, pk_object* object) {
    object->gc.refcnt--;
    if (object->gc.refcnt == 0) {
        // Free it now, no other references
        pk_finalize(vm, object);
        // Put it in the tombstone list
        object->payload = (void*)vm->gc.tombstones;
        vm->gc.tombstones = object;
    }
}

pk_vm* pk_new(void) {
    pk_vm* vm = (pk_vm*)calloc(1, sizeof(struct pk_vm));
    vm->global_scope = pk_alloc_object(vm, PK_TYPE_NONE);
    return vm;
}

// ------------------------- Hashmap stuff -------------------------

inline pk_hashmap* pk_hashmap_new(void) {
    return (pk_hashmap*)calloc(1, sizeof(struct pk_hashmap));
}

unsigned int pk_hashmap_hash(const char* value) {
    unsigned int hash = 5381;
    for (char c = *value; c; c = *value++) {
        hash = (hash << 5) + hash + c;
    }
    return hash & PK_HASHMAP_BUCKETMASK;
}
    
void pk_hashmap_destroy(pk_vm* vm, pk_hashmap* map) {
    for (size_t b = 0; b < PK_HASHMAP_BUCKETS; b++) {
        pk_hashbucket bucket = map->buckets[b];
        for (size_t e = 0; e < bucket.cap; e++) {
            free(bucket.entries[e].key);
            pk_decref(vm, bucket.entries[e].value);
        }
    }
    free(map);
}
    
void pk_hashmap_put(pk_vm* vm, pk_hashmap* map, const char* key, pk_object* value, bool readonly) {
    pk_incref(value);
    pk_hashbucket b = map->buckets[pk_hashmap_hash(key)];
    for (size_t i = 0; i < b.cap; i++) {
        if (streq(b.entries[i].key, key)) {
            b.entries[i].value = value;
            b.entries[i].readonly = readonly;
            return;
        }
    }
    b.entries = (pk_hashentry*)realloc(b.entries, sizeof(struct pk_hashentry) * (b.cap + 1));
    b.entries[b.cap].key = strdup(key);
    b.entries[b.cap].value = value;
    b.cap++;
}

#ifdef DEBUG
int main(void) {
    printf("%lu %lu %lu\nfoobarbaz", sizeof(struct pk_object), sizeof(struct pk_vm), sizeof(struct pk_hashmap));
    return 0;
}
#endif
    
#ifdef __cplusplus
}
#endif
