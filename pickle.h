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
typedef uint16_t pickle_type_t;
typedef uint16_t pickle_flags_t;
typedef uint16_t pickle_resultcode_t;
typedef struct pickle_object* pickle_object_t;
typedef struct pickle_vm* pickle_vm_t;
typedef struct pickle_parser* pickle_parser_t;
typedef struct pickle_hashmap* pickle_hashmap_t;

typedef pickle_object_t (*pickle_builtin_function_t)(pickle_vm_t, pickle_object_t, size_t, pickle_object_t*);

// These function only operate on the void* payload of the object, everything else is handles automatically
typedef void (*pickle_type_function_t)(pickle_vm_t, pickle_object_t);
#define PICKLE_NUMTYPEFUNS 3
#define PICKLE_INITFUN 0
#define PICKLE_MARKFUN 1
#define PICKLE_FINALFUN 2

typedef void (*pickle_exit_callback_t)(pickle_vm_t, pickle_object_t);
typedef void (*pickle_write_callback_t)(pickle_vm_t, const char*);
typedef char* (*pickle_read_callback_t)(pickle_vm_t, const char*);
typedef char* (*pickle_source_callback_t)(pickle_vm_t, const char*);
typedef void (*pickle_store_callback_t)(pickle_vm_t, const char*, const char*);
typedef void (*pickle_error_callback_t)(pickle_vm_t, size_t, const char*);
typedef const char* (*pickle_embeddedfilter_callback_t)(pickle_vm_t, const char*);
typedef const char* (*pickle_checkinterrupt_callback_t)(pickle_vm_t);
    
#define streq(a, b) (!strcmp(a, b))

#define PICKLE_TYPE_TOMBSTONE UINT16_MAX
#define PICKLE_TYPE_NONE 0

#define PICKLE_HASHMAP_BUCKETS 256
#define PICKLE_HASHMAP_BUCKETMASK 0xFF
typedef struct pickle_hashentry {
    char* key;
    pickle_object_t value;
    bool readonly;
} pickle_hashentry;

typedef struct pickle_hashbucket {
    pickle_hashentry* entries;
    size_t cap;
} pickle_hashbucket;

struct pickle_hashmap {
    pickle_hashbucket buckets[PICKLE_HASHMAP_BUCKETS];
};

struct pickle_object {
    pickle_type_t type;
    struct {
        pickle_object_t next;
        size_t refcnt;
    } gc;
    struct {
        pickle_flags_t global;
        pickle_flags_t obj;
    } flags;
    struct {
        pickle_object_t* bases;
        size_t cap;
        size_t len;
    } proto;
    pickle_hashmap_t properties;
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
//             pickle_object_t* items;
//             size_t len;
//             size_t cap;
//         } as_list;
//         pickle_hashmap_t as_hashmap;
//         union {
//             pickle_builtin_function_t c_func;
//             struct {
//                 const char** argnames;
//                 size_t argc;
//                 pickle_object_t closure;
//             } user;
//         } as_func;
//         const char* as_string;
//         struct {
//             const char* message;
//             // Leave room for tracebacks, if I ever implement that
//         } as_error;
//         struct {
//             pickle_object_t result;
//             pickle_resultcode_t resultcode;
//         } as_scope;
//         struct {
//             pickle_object_t* code;
//             size_t len;
//             size_t cap;
//         } as_code;
//         struct {
//             pickle_object_t car;
//             pickle_object_t cdr;
//         } as_cons;
//     } payload;
};

typedef struct pickle_typemgr {
    pickle_type_function_t funs[PICKLE_NUMTYPEFUNS];
    pickle_type_t type_for;
    const char* type_string;
} pickle_typemgr;
    
typedef struct pickle_operator {
    const char* symbol;
    const char* method;
    int precedence;
} pickle_operator;

struct pickle_parser {
    pickle_parser_t parent;
    const char* code;
    size_t len;
    size_t head;
    bool ignore_eol;
    size_t depth;
};

struct pickle_vm {
    struct {
        pickle_object_t first;
        pickle_object_t tombstones;
    } gc;
    struct {
        pickle_typemgr* mgrs;
        size_t len;
        size_t cap;
    } type_managers;
    struct {
        pickle_operator* ops;
        size_t len;
        size_t cap;
    } operators;
    pickle_parser_t parser;
    pickle_object_t global_scope;
    pickle_object_t dollar_function;
    struct {
        pickle_exit_callback_t exit;
        pickle_write_callback_t write;
        pickle_read_callback_t read;
        pickle_source_callback_t source;
        pickle_store_callback_t store;
        pickle_error_callback_t error;
        pickle_embeddedfilter_callback_t embeddedfilter;
        pickle_checkinterrupt_callback_t checkinterrupt;
    } callbacks;
};

#define PICKLE_DOCALLBACK(vm, name, ...) \
if (vm->callbacks.name != NULL) { \
    vm->callbacks.name(__VA_ARGS__);\
}

// Forward references
void pickle_register_globals(pickle_vm_t);
void pickle_hashmap_destroy(pickle_vm_t, pickle_hashmap_t);
void pickle_decref(pickle_vm_t, pickle_object_t);

// ------------------- Alloc/dealloc objects -------------------------
    
void pickle_run_typefun(pickle_vm_t vm, pickle_type_t type, pickle_object_t object, int name) {
    for (size_t i = 0; i < vm->type_managers.len; i++) { 
        if (vm->type_managers.mgrs[i].type_for == type && vm->type_managers.mgrs[i].funs[name] != NULL) { 
            vm->type_managers.mgrs[i].funs[name](vm, object); 
            return; 
        } 
    }
}

pickle_object_t pickle_alloc_object(pickle_vm_t vm, pickle_type_t type) {
    pickle_object_t object;
    if (vm->gc.tombstones == NULL) {
        object = (pickle_object_t)calloc(1, sizeof(struct pickle_object));
        object->gc.next = vm->gc.first;
        vm->gc.first = object;
    } else {
        object = vm->gc.tombstones;
        vm->gc.tombstones = (pickle_object_t)object->payload;
    }
    object->type = type;
    object->gc.refcnt = 1;
    pickle_run_typefun(vm, type, object, PICKLE_INITFUN);
    return object;
}
    

void pickle_finalize(pickle_vm_t vm, pickle_object_t object) {
    pickle_type_t type = object->type;
    object->type = PICKLE_TYPE_TOMBSTONE;
    // Free object-specific stuff
    pickle_run_typefun(vm, type, object, PICKLE_FINALFUN);
    // Free everything else
    object->flags.global = 0;
    object->flags.obj = 0;
    for (size_t i = 0; i < object->proto.len; i++) {
        pickle_decref(vm, object->proto.bases[i]);
    }
    free(object->proto.bases);
    object->proto.bases = NULL;
    object->proto.len = 0;
    object->proto.cap = 0;
    pickle_hashmap_destroy(vm, object->properties);
    object->properties = NULL;
}

#define pickle_incref(object) ((object)->gc.refcnt++)

void pickle_decref(pickle_vm_t vm, pickle_object_t object) {
    object->gc.refcnt--;
    if (object->gc.refcnt == 0) {
        // Free it now, no other references
        pickle_finalize(vm, object);
        // Put it in the tombstone list
        object->payload = (void*)vm->gc.tombstones;
        vm->gc.tombstones = object;
    }
}

pickle_vm_t pickle_new(void) {
    pickle_vm_t vm = (pickle_vm_t)calloc(1, sizeof(struct pickle_vm));
    vm->global_scope = pickle_alloc_object(vm, PICKLE_TYPE_NONE);
    return vm;
}

// ------------------------- Hashmap stuff -------------------------

inline pickle_hashmap_t pickle_hashmap_new(void) {
    return (pickle_hashmap_t)calloc(1, sizeof(struct pickle_hashmap));
}

unsigned int pickle_hashmap_hash(const char* value) {
    unsigned int hash = 5381;
    for (char c = *value; c; c = *value++) {
        hash = (hash << 5) + hash + c;
    }
    return hash & PICKLE_HASHMAP_BUCKETMASK;
}
    
void pickle_hashmap_destroy(pickle_vm_t vm, pickle_hashmap_t map) {
    for (size_t b = 0; b < PICKLE_HASHMAP_BUCKETS; b++) {
        pickle_hashbucket bucket = map->buckets[b];
        for (size_t e = 0; e < bucket.cap; e++) {
            free(bucket.entries[e].key);
            pickle_decref(vm, bucket.entries[e].value);
        }
    }
    free(map);
}
    
void pickle_hashmap_put(pickle_vm_t vm, pickle_hashmap_t map, const char* key, pickle_object_t value, bool readonly) {
    pickle_incref(value);
    pickle_hashbucket b = map->buckets[pickle_hashmap_hash(key)];
    for (size_t i = 0; i < b.cap; i++) {
        if (streq(b.entries[i].key, key)) {
            b.entries[i].value = value;
            b.entries[i].readonly = readonly;
            return;
        }
    }
    b.entries = (pickle_hashentry*)realloc(b.entries, sizeof(struct pickle_hashentry) * (b.cap + 1));
    b.entries[b.cap].key = strdup(key);
    b.entries[b.cap].value = value;
    b.cap++;
}

#ifdef DEBUG
int main(void) {
    printf("%lu %lu %lu\nfoobarbaz", sizeof(struct pickle_object), sizeof(struct pickle_vm), sizeof(struct pickle_hashmap));
    return 0;
}
#endif
    
#ifdef __cplusplus
}
#endif
