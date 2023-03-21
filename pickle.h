#ifndef PICKLE_H
#define PICKLE_H


#include <stdlib.h>
#include <stdarg.h>
#include <stdio.h>
#include <string.h>
#include <ctype.h>
#include <stdint.h>

#include <vector>

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

#if 1
    #define PIK_DEBUG_PRINTF(fmt, ...) printf("[DEBUG %s:%i] " fmt, __FILE__, __LINE__, ##__VA_ARGS__)
    #define PIK_DEBUG_ASSERT(cond, should) __PIK_DEBUG_ASSERT_INNER(cond, should, #cond, __FILE__, __LINE__, __func__)
    void __PIK_DEBUG_ASSERT_INNER(bool cond, const char* should, const char* condstr, const char* filename, size_t line, const char* func) {
        printf("[%s:%zu in %s] Assertion %s: %s\n", filename, line, func, cond ? "succeeded" : "failed", condstr);
        if (cond) return;
        printf("%s\nAbort.", should);
        exit(70);
        // *((int*)0) = 1; // Segfault fast
    }
#else
    #define PIK_DEBUG_PRINTF(...)
    #define PIK_DEBUG_ASSERT(...)
#endif

enum {
    PICKLE_MARKBIT,
    PICKLE_FINALIZED
};

class PickleObject;

class PickleHashmapEntry {
    private:
    char* key;
    PickleObject* value;
    PickleHashmapEntry(const char* key, PickleObject* value);
    public:
    ~PickleHashmapEntry();
    friend class PickleHashmapBucket;
};

class PickleHashmapBucket {
    private:
    std::vector<PickleHashmapEntry> entries;
    PickleHashmapBucket();
    public:
    ~PickleHashmapBucket();
    void mark();
    void clear();
    friend class PickleHashmap;
};

class PickleHashmap {
    private:
    PickleHashmapBucket buckets[256];
    public:
    PickleHashmap();
    void mark();
    void clear();
};

typedef void (*PickleInitfun)(PickleObject*, void*);
typedef void (*PickleMarkfun)(PickleObject*);
typedef void (*PickleFreefun)(PickleObject*);
class PickleType {
    public:
    PickleInitfun initfun;
    PickleMarkfun markfun;
    PickleFreefun freefun;
    char* name;
    PickleType(const char* name, const PickleInitfun init, const PickleMarkfun mark, const PickleFreefun free);
    ~PickleType();
    void init(PickleObject* foo, void* arg);
    void mark(PickleObject* foo);
    void free(PickleObject* foo);
    operator bool();
};

class PickleObject {
    private:
    size_t refs;
    PickleObject* next;
    uint32_t flags;
    std::vector<PickleObject*> superclasses;
    PickleHashmap* properties;
    const PickleType* type;
    public:
    union {
        void* pointer;
        char* string;
        int64_t intnum;
        double floatnum;
        struct {
            union {
                void* car;
                int32_t numerator;
                float real;
            };
            union {
                void* cdr;
                uint32_t denominator;
                float imag;
            };
        };
    };
    private:
    PickleObject(const PickleType* type, void* arg, const PickleObject* prev);
    PickleObject(const PickleType* type, void* arg);
    PickleObject(const PickleType* t);
    PickleObject();
    public:
    ~PickleObject();
    inline void setflag(uint32_t flag);
    inline void clrflag(uint32_t flag);
    inline bool tstflag(uint32_t flag);
    inline void incref();
    void decref();
    PickleObject& operator=(PickleObject& other);
    void finalize();
    void mark();
    operator bool();
    const char* mytype();
    friend class PickleVM;
};

class PickleVM {
    private:
    PickleObject* first;
    size_t allocs;
    PickleObject* global_scope;
    std::vector<PickleType*> types;
    public:
    PickleVM();
    ~PickleVM();
    private:
    void mark_all();
    void sweep_unmarked();
    public:
    void gc();
    void register_type(PickleType* type);
    private:
    PickleType* find_type(const char* type);
    public:
    PickleObject* alloc(const char* type, void* arg);
};


#include "pickle.cpp"

#endif
