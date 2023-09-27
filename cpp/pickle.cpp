#include "pickle.hpp"

namespace pickle {

using tinobsy::cell;

char unescape(char c) {
    switch (c) {
        case 'b': return '\b';
        case 't': return '\t';
        case 'n': return '\n';
        case 'v': return '\v';
        case 'f': return '\f';
        case 'r': return '\r';
        case 'a': return '\a';
        case 'o': return '{';
        case 'c': return '}';
        case '\n': return 0;
        default: return c;
    }
}

char escape(char c) {
    switch (c) {
        case '\b': return 'b';
        case '\t': return 't';
        case '\n': return 'n';
        case '\v': return 'v';
        case '\f': return 'f';
        case '\r': return 'r';
        case '\a': return 'a';
        case '{': return 'o';
        case '}': return 'c';
        default: return c;
    }
}

bool needs_escape(char c) {
    return strchr("{}\b\t\n\v\f\r\a\\\"", c) != NULL;
}

location::location() {}

location::location(size_t line, size_t col)
: line(line),
  col(col) {}

static void init_metadata(object* self, va_list args) {
    self->cells = new cell[2];
    self->cells[0].as_ptr = (void*)(new location(va_arg(args, location*)));
    self->cells[1].as_obj = va_arg(args, object*);
}

static void mark_metadata(object* self) {
    self->cells[1].as_obj->mark();
}

static void finalize_metadata(object* self) {
    delete (location*)(self->cells[0].as_ptr);
}

static void init_c_function(object* self, va_list args) {
    self->as_ptr = (void*)va_arg(args, func_ptr);
}

static void init_function_partial(object* self, va_list args) {
    self->cells = new cell[5];
    self->cells[0].as_obj = va_arg(args, object*); // function
    self->cells[1].as_obj = va_arg(args, object*); // args
    self->cells[2].as_obj = va_arg(args, object*); // env
    self->cells[3].as_obj = va_arg(args, object*); // cont
    self->cells[4].as_obj = va_arg(args, object*); // failcont
}

static void mark_function_partial(object* self) {
    for (int i = 0; i < 5; i++) self->cells[i].as_obj->mark();
}

const object_schema metadata_type("object_metadata", init_metadata, NULL, mark_metadata, finalize_metadata);
const object_schema cons_type("cons", tinobsy::schema_functions::init_cons, NULL, tinobsy::schema_functions::mark_cons, tinobsy::schema_functions::finalize_cons);
const object_schema partial_type("function_partial", init_function_partial, NULL, mark_metadata, NULL);
const object_schema c_function_type("c_function", init_c_function, NULL, NULL, NULL);
const object_schema string_type("string", tinobsy::schema_functions::init_str, tinobsy::schema_functions::cmp_str, NULL, tinobsy::schema_functions::finalize_str);

}
