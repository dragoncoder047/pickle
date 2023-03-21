#include "pickle.h"
#pragma once

PickleHashmapEntry::PickleHashmapEntry(const char* key, PickleObject* value) {
    this->key = strdup(key);
    this->value = value;
    value->incref();
}
PickleHashmapEntry::~PickleHashmapEntry() {
    delete this->key;
    this->value->decref();
}

PickleHashmapBucket::PickleHashmapBucket() {}
PickleHashmapBucket::~PickleHashmapBucket() {
    for (auto entry : this->entries) entry.value->decref();
}
void PickleHashmapBucket::mark() {
    for (auto entry : this->entries) entry.value->mark();
}

void PickleHashmap::mark() {
    for (auto bucket : this->buckets) bucket.mark();
}

PickleType::PickleType(const char* name, const PickleTypefun init, const PickleTypefun mark, const PickleTypefun free) {
    this->name = strdup(name);
    this->init = init;
    this->mark = mark;
    this->free = free;
}
PickleType::~PickleType() {
    delete this->name;
}

PickleObject::PickleObject(const PickleType* type) {
    this->type = type;
    this->properties = new PickleHashmap;
    this->incref();
}
PickleObject::PickleObject() {
    PickleObject(NULL);
}
PickleObject::~PickleObject() {
    this->finalize();
}
void PickleObject::decref() {
    if (this->tstflag(PICKLE_FINALIZED)) return;
    this->refs--;
    if (this->refs == 0) this->finalize();
}
void PickleObject::finalize() {
    this->type->free(this);
    for (auto sup : this->superclasses) sup->decref();
    delete this->properties;
    this->flags = 1<<PICKLE_FINALIZED;
    // Also clears marked flag
}

void PickleObject::mark() {
    if (this->tstflag(PICKLE_MARKBIT)) return;
    this->setflag(PICKLE_MARKBIT);
    this->properties->mark();
    for (auto sup : this->superclasses) sup->mark();
    this->type->mark(this);
}
PickleObject::operator bool() {
    return this != NULL;
}

PickleVM::PickleVM() {
    this->first = NULL;
    this->allocs = 0;
}
PickleVM::~PickleVM() {
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
    this->mark_all();
    this->sweep_unmarked();
}