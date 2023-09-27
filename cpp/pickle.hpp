#ifndef PICKLE_H
#define PICKLE_H

#include "tinobsy/tinobsy.hpp"
#include <cstdlib>
#include <cctype>
#include <cstdint>
#include <cstring>
#include <cstdarg>

namespace pickle {

using tinobsy::object, tinobsy::object_schema;

char escape(char c);
char unescape(char c);
bool needs_escape(char c);

// A struct to hold the line/column information for tokens.
class location {
    public:
    location();
    location(size_t line, size_t col);
    size_t line = 1;
    size_t col = 1;
};

class pickle : public tinobsy::vm;

typedef void (*func_ptr)(pickle* runner, object* args, object* env, object* cont, object* fail_cont);


extern const object_schema metadata_type;
extern const object_schema cons_type;
extern const object_schema partial_type;
extern const object_schema c_function_type;
extern const object_schema string_type;

}

#define car(x) ((x)->cells[0].as_obj)
#define cdr(x) ((x)->cells[1].as_obj)

#include "pickle.cpp"

#endif
