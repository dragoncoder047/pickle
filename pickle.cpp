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
    PIK_DEBUG_PRINTF("new PickleHashmapBucket()\n");
}
PickleHashmapBucket::~PickleHashmapBucket() {
    PIK_DEBUG_PRINTF("~PickleHashmapBucket()\n");
    for (auto entry : this->entries) entry.value->decref();
}
void PickleHashmapBucket::mark() {
    PIK_DEBUG_PRINTF("PickleHashmapBucket::mark()\n");
    for (auto entry : this->entries) entry.value->mark();
}

void PickleHashmap::mark() {
    PIK_DEBUG_PRINTF("PickleHashmap::mark()\n");
    for (auto bucket : this->buckets) bucket.mark();
}

PickleType::PickleType(const char* name, const PickleTypefun init, const PickleTypefun mark, const PickleTypefun free) {
    PIK_DEBUG_PRINTF("new PickleType(\"%s\", %p, %p, %p)\n", name, init, mark, free);
    this->name = strdup(name);
    this->init = init;
    this->mark = mark;
    this->free = free;
}
PickleType::~PickleType() {
    PIK_DEBUG_PRINTF("~PickleType(\"%s\")\n", this->name);
    delete this->name;
}
PickleType::operator bool() {
    return this != NULL;
}

PickleObject::PickleObject(const PickleType* type) {
    this->type = type;
    PIK_DEBUG_PRINTF("new PickleObject(\"%s\")\n", this->mytype());
    this->properties = new PickleHashmap;
    this->incref();
}
PickleObject::PickleObject() {
    PickleObject(NULL);
}
PickleObject::~PickleObject() {
    PIK_DEBUG_PRINTF("~PickleObject(\"%s\")\n", this->mytype());
    this->finalize();
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
void PickleObject::finalize() {
    PIK_DEBUG_PRINTF("PickleObject::finalize(\"%s\")\n", this->mytype());
    for (auto sup : this->superclasses) sup->decref();
    this->type->free(this);
    delete this->properties;
    this->flags = 1<<PICKLE_FINALIZED;
    // Also clears marked flag
}
void PickleObject::mark() {
    if (this->tstflag(PICKLE_MARKBIT)) return;
    PIK_DEBUG_PRINTF("PickleObject::mark(\"%s\")\n", this->mytype());
    this->setflag(PICKLE_MARKBIT);
    this->properties->mark();
    for (auto sup : this->superclasses) sup->mark();
    this->type->mark(this);
}
PickleObject::operator bool() {
    return this != NULL;
}
const char* PickleObject::mytype() {
    if (!this->type) return "object";
    return this->type->name;
}

PickleVM::PickleVM() {
    PIK_DEBUG_PRINTF("new PickleVM()\n");
    this->first = NULL;
    this->allocs = 0;
}
PickleVM::~PickleVM() {
    PIK_DEBUG_PRINTF("~PickleVM()\n");
    PickleObject* x = this->first;
    while (x) {
        PickleObject* y = x;
        x = x->next;
        delete y;
        this->allocs--;
    }
    this->global_scope = NULL;
}
void PickleVM::mark_all() {
    if (this->global_scope) this->global_scope->mark();
}
void PickleVM::sweep_unmarked() {
    PickleObject** object = &this->first;
    while (*object) {
        if ((*object)->tstflag(PICKLE_MARKBIT)) {
            // Keep the object
            (*object)->clrflag(PICKLE_MARKBIT);
            object = &(*object)->next;
        } else {
            // Sweep the object
            PickleObject* unreached = *object;
            *object = unreached->next;
            delete object;
            this->allocs--;
        }
    }
}

void PickleVM::gc() {
    PIK_DEBUG_PRINTF("PickleVM::gc()\n");
    this->mark_all();
    this->sweep_unmarked();
}
