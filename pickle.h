#include <stdlib.h>
#include <stdio.h>
#include <stdint.h>
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
typedef void (*pickle_init_function_t)(pickle_vm_t, pickle_object_t);
typedef void (*pickle_finalize_function_t)(pickle_vm_t, pickle_object_t);
typedef void (*pickle_mark_function_t)(pickle_vm_t, pickle_object_t);

typedef void (*pickle_exit_callback_t)(pickle_vm_t, pickle_object_t);
typedef void (*pickle_write_callback_t)(pickle_vm_t, const char*);
typedef char* (*pickle_read_callback_t)(pickle_vm_t, const char*);
typedef char* (*pickle_source_callback_t)(pickle_vm_t, const char*);
typedef void (*pickle_store_callback_t)(pickle_vm_t, const char*, const char*);
typedef void (*pickle_error_callback_t)(pickle_vm_t, size_t, const char*);
typedef const char* (*pickle_embeddedfilter_callback_t)(pickle_vm_t, const char*);
typedef const char* (*pickle_checkinterrupt_callback_t)(pickle_vm_t);

#define PICKLE_TYPE_TOMBSTONE UINT16_MAX
#define PICKLE_TYPE_NONE 0

#define PICKLE_HASHMAP_BUCKETS 256
#define PICKLE_HASHMAP_BUCKETMASK 0xFF
typedef struct pickle_hashentry {
    char* key;
    pickle_object_t value;
} pickle_hashentry;

typedef struct pickle_hashbucket {
    pickle_hashentry* entries;
    size_t capacity;
} pickle_hashbucket;

struct pickle_hashmap {
    pickle_hashbucket cells[PICKLE_HASHMAP_BUCKETS];
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
    pickle_finalize_function_t finalize;
    pickle_init_function_t initialize;
    pickle_mark_function_t mark;
    pickle_type_t type_for;
    const char* type_string;
} pickle_typemgr;

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
        pickle_typemgr* funs;
        size_t len;
        size_t cap;
    } type_managers;
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
if (vm->callbacks.name) { \
    vm->callbacks.name(__VA_ARGS__);\
}

#define PICKLE_DOTYPEMGR(vm, type, object, name) \
for (size_t i = 0; i < vm->type_managers.len; i++) { \
    if (vm->type_managers.funs[i].type_for == type && vm->type_managers.funs[i].name != NULL) { \
        vm->type_managers.funs[i].name(vm, object); \
        break; \
    } \
}

// Forward references
void pickle_register_globals(pickle_vm_t);
void pickle_hashmap_destroy(pickle_vm_t, pickle_hashmap_t);
void pickle_decref(pickle_vm_t, pickle_object_t);


// ------------------- Alloc/dealloc objects -------------------------

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
    PICKLE_DOTYPEMGR(vm, type, object, initialize);
    return object;
}
    

void pickle_finalize(pickle_vm_t vm, pickle_object_t object) {
    pickle_type_t type = object->type;
    object->type = PICKLE_TYPE_TOMBSTONE;
    // Free object-specific stuff
    PICKLE_DOTYPEMGR(vm, type, object, finalize);
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
    // pickle_hashmap_destroy(vm, object->properties);
    // object->properties = NULL;
}

inline void pickle_incref(pickle_object_t object) {
    object->gc.refcnt++;
}

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
    //for (
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
