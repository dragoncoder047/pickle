#include "pickle.h"
#pragma once

PickleHashmapEntry::PickleHashmapEntry(const char* key, PickleObject* value) {
    PIK_DEBUG_PRINTF("new PickleHashmapEntry(\"%s\", %s)\n", key, value->mytype());
    this->key = strdup(key);
    this->value = value;
    value->incref();
}
PickleHashmapEntry::~PickleHashmapEntry() {
    PIK_DEBUG_PRINTF("~PickleHashmapEntry(\"%s\", %s)\n", this->key, this->value->mytype());
    delete this->key;
    this->value->decref();
}

PickleHashmapBucket::PickleHashmapBucket() {
}
PickleHashmapBucket::~PickleHashmapBucket() {
    for (auto entry : this->entries) entry.value->decref();
}
void PickleHashmapBucket::mark() {
    for (auto entry : this->entries) entry.value->mark();
}
void PickleHashmapBucket::clear() {
    this->entries.clear();
}

PickleHashmap::PickleHashmap() {
    PIK_DEBUG_PRINTF("new PickleHashmap()\n");
}
void PickleHashmap::mark() {
    PIK_DEBUG_PRINTF("PickleHashmap::mark()\n");
    for (auto bucket : this->buckets) bucket.mark();
}
void PickleHashmap::clear() {
    PIK_DEBUG_PRINTF("PickleHashmap::clear()\n");
    for (auto bucket : this->buckets) bucket.clear();
}

PickleType::PickleType(const char* name, const PickleInitfun init, const PickleMarkfun mark, const PickleFreefun free) {
    PIK_DEBUG_PRINTF("new PickleType(\"%s\", %p, %p, %p)\n", name, init, mark, free);
    this->name = strdup(name);
    this->initfun = init;
    this->markfun = mark;
    this->freefun = free;
}
PickleType::~PickleType() {
    PIK_DEBUG_PRINTF("~PickleType(\"%s\")\n", this->name);
    std::free(this->name);
}
void PickleType::init(PickleObject* foo, void* arg) {
    if (!foo || !this->initfun) return;
    this->initfun(foo, arg);
}
void PickleType::mark(PickleObject* foo) {
    if (!foo || !this->markfun) return;
    this->markfun(foo);
}
void PickleType::free(PickleObject* foo) {
    if (!foo || !this->freefun) return;
    this->freefun(foo);
}
PickleType::operator bool() {
    return this != NULL;
}

PickleObject::PickleObject(const PickleType* type, void* arg, const PickleObject* prev) {
    this->type = type;
    PIK_DEBUG_PRINTF("new PickleObject(\"%s\")\n", this->mytype());
    this->properties = new PickleHashmap;
    if (type) type->init((PickleObject*)this, arg);
    this->refs = 1;
    this->next = (PickleObject*)prev;
    this->flags = 0;
}
PickleObject::PickleObject(const PickleType* type, void* arg) {
    PickleObject(type, arg, NULL);
}
PickleObject::PickleObject(const PickleType* type) {
    PickleObject(type, NULL, NULL);
}
PickleObject::PickleObject() {
    PickleObject(NULL, NULL, NULL);
}
PickleObject::~PickleObject() {
    PIK_DEBUG_PRINTF("~PickleObject(\"%s\")\n", this->mytype());
    this->finalize();
    delete this->properties;
}
inline void PickleObject::setflag(uint32_t flag) {
    this->flags |= 1<<flag;
}
inline void PickleObject::clrflag(uint32_t flag) {
    this->flags &= ~(1<<flag);
}
inline bool PickleObject::tstflag(uint32_t flag) {
    return this->flags & (1<<flag);
}
inline void PickleObject::incref() {
    this->refs++;
    PIK_DEBUG_PRINTF("PickleObject::incref(\"%s\") to %zu\n", this->mytype(), this->refs);
}
void PickleObject::decref() {
    if (this->tstflag(PICKLE_FINALIZED)) return;
    this->refs--;
    PIK_DEBUG_PRINTF("PickleObject::decref(\"%s\") to %zu\n", this->mytype(), this->refs);
    if (this->refs == 0) this->finalize();
}
// Operator overloading reference counting magic!!!!
PickleObject& PickleObject::operator=(PickleObject& other) {
    if (this == &other) return *this; // Short circuit on self-assignment
    PIK_DEBUG_PRINTF("PickleObject::operator=(\"%s\") << (\"%s\")\n", this->mytype(), other.mytype());
    other.incref();
    this->decref();
    return other;
}
void PickleObject::finalize() {
    PIK_DEBUG_PRINTF("PickleObject::finalize(\"%s\")\n", this->mytype());
    for (auto sup : this->superclasses) sup->decref();
    this->superclasses.clear();
    if (this->type) this->type->free((PickleObject*)this);
    this->type = NULL;
    this->flags = 1<<PICKLE_FINALIZED;
    // Also clears marked flag
    this->properties->clear();
}
void PickleObject::mark() {
    if (!this) return;
    if (this->tstflag(PICKLE_MARKBIT)) return;
    PIK_DEBUG_PRINTF("PickleObject::mark(\"%s\")\n", this->mytype());
    this->setflag(PICKLE_MARKBIT);
    this->properties->mark();
    for (auto sup : this->superclasses) sup->mark();
    if (this->type) this->type->mark((PickleObject*)this);
}
PickleObject::operator bool() {
    PIK_DEBUG_PRINTF("PickleObject::operator bool() -> %s\n", this != NULL ? "true" : "false");
    return this != NULL;
}
const char* PickleObject::mytype() {
    if (!this) return "NULL";
    if (!this->type) return "object";
    return this->type->name;
}

PickleVM::PickleVM() {
    PIK_DEBUG_PRINTF("new PickleVM()\n");
    this->first = NULL;
    this->allocs = 0;
    this->global_scope = NULL;
}
PickleVM::~PickleVM() {
    PIK_DEBUG_PRINTF("~PickleVM()\n");
    this->global_scope = NULL;
    this->gc();
    PIK_DEBUG_ASSERT(this->allocs == 0, "failed to free all in gc");
    for (auto t : this->types) delete t;
}
void PickleVM::mark_all() {
    PIK_DEBUG_PRINTF("PickleVM::mark_all()\n");
    if (this->global_scope) this->global_scope->mark();
}
void PickleVM::sweep_unmarked() {
    PIK_DEBUG_PRINTF("PickleVM::sweep_unmarked()\n");
    PickleObject** object = &this->first;
    while (*object) {
        PIK_DEBUG_PRINTF("Looking at %s at %p: \n", (*object)->mytype(), (void*)(*object));
        if ((*object)->tstflag(PICKLE_MARKBIT)) {
            PIK_DEBUG_PRINTF("Marked\n");
            // Keep the object
            (*object)->clrflag(PICKLE_MARKBIT);
            object = &(*object)->next;
        } else {
            PIK_DEBUG_PRINTF("Unmarked\n");
            // Sweep the object
            PickleObject* unreached = *object;
            *object = unreached->next;
            delete unreached;
            this->allocs--;
        }
    }
}
void PickleVM::gc() {
    PIK_DEBUG_PRINTF("PickleVM::gc()\n");
    this->mark_all();
    this->sweep_unmarked();
}
void PickleVM::register_type(PickleType* type) {
    PIK_DEBUG_PRINTF("PickleVM::register_type(\"%s\")\n", type->name);
    this->types.push_back(type);
}
PickleType* PickleVM::find_type(const char* type) {
    for (auto x : this->types) {
        if (streq(type, x->name)) return x;
    }
    return NULL;
}
PickleObject* PickleVM::alloc(const char* type, void* arg) {
    PIK_DEBUG_PRINTF("PickleVM::alloc(\"%s\"): ", type);
    PickleType* t = this->find_type(type);
    PickleObject* o = this->first;
    while (o) {
        if (o->tstflag(PICKLE_FINALIZED)) {
            PIK_DEBUG_PRINTF("garbage\n");
            goto got_garbage;
            o->type = t;
            t->init(o, arg);
            o->refs = 1;
        }
        o = o->next;
    }
    PIK_DEBUG_PRINTF("new alloc\n");
    o = new PickleObject(t, arg, this->first);
    this->first = o;
    this->allocs++;
    got_garbage:
    return o;
}
