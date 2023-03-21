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
    friend class PickleHashmap;
};

class PickleHashmap {
    private:
    PickleHashmapBucket buckets[256];
    public:
    void mark();
};

typedef void (*PickleTypefun)(PickleObject*);
class PickleType {
    public:
    PickleTypefun init;
    PickleTypefun mark;
    PickleTypefun free;
    char* name;
    PickleType(const char* name, const PickleTypefun init, const PickleTypefun mark, const PickleTypefun free);
    ~PickleType();
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
    PickleObject(const PickleType* t);
    PickleObject();
    public:
    ~PickleObject();
    inline void setflag(uint32_t flag) {
        this->flags |= 1<<flag;
    }
    inline void clrflag(uint32_t flag) {
        this->flags &= ~(1<<flag);
    }
    inline bool tstflag(uint32_t flag) {
        return this->flags & (1<<flag);
    }
    inline void incref() {
        this->refs++;
    }
    void decref();
    void finalize();
    void mark();
    operator bool();
    friend class PickleVM;
};

class PickleVM {
    private:
    PickleObject* first;
    size_t allocs;
    PickleObject* global_scope;
    std::vector<PickleType> types;
    public:
    PickleVM();
    ~PickleVM();
    private:
    void mark_all();
    void sweep_unmarked();
    public:
    void gc();
};


#include "pickle.cpp"

#endif