#include "pickle.hpp"
#include <errno.h>
#include <inttypes.h>

namespace pickle {

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

void free_payload(object* o) { free(o->as_ptr); }

// ------------------------ core types -----------------
// these will later be swapped for actual objects

// cons = car, cdr
const object_type cons_type("cons", tinobsy::markcons, NULL, NULL);
// --------- primitive/ish types ---------------
const object_type string_type("string", NULL, free_payload, NULL);
const object_type symbol_type("symbol", NULL, free_payload, NULL);
const object_type c_function_type("c_function", NULL, NULL, NULL);
const object_type integer_type("int", NULL, NULL, NULL);
const object_type float_type("float", NULL, NULL, NULL);
const object_type* primitives[] = { &string_type, &symbol_type, &c_function_type, &integer_type, &float_type, NULL };

void pickle::mark_globals() {
    this->markobject(this->queue_head);
    this->markobject(this->queue_tail);  // in case queue gets detached
    this->markobject(this->globals);
    this->markobject(this->stack);
    this->markobject(this->instruction_stack);
}

void pickle::step() {
    DBG("TODO: write step stack code");
}

//--------------- HELPER FUNCTIONS ----------------------------

static bool is_primitive_type(object* x) {
    if (x == NULL) return true;
    size_t i = 0; while (primitives[i] != NULL) { if (x->type == primitives[i]) break; i++; }
    return primitives[i] != NULL;
}

int eqcmp(object* a, object* b) {
    if (a == b) return 0;
    if (a == NULL) return -1;
    if (b == NULL) return 1;
    if (a->type != b->type) return a->type - b->type;
    if (!is_primitive_type(a)) return -1;
    if (a->type->free) return strcmp(a->as_chars, b->as_chars); // String-ish type
    if (a->type == &float_type) return a->as_double - b->as_double;
    return a->as_big_int - b->as_big_int;
}

object* assoc(object* list, object* key) {
    for (; list; list = cdr(list)) {
        object* pair = car(list);
        if (!eqcmp(key, car(pair))) return pair;
    }
    return NULL;
}

object* delassoc(object** list, object* key) {
    for (; *list; list = &cdr(*list)) {
        object* pair = car(*list);
        if (!eqcmp(key, car(pair))) {
            *list = cdr(*list);
            return pair;
        }
    }
    return NULL;
}

//--------------- PARSER --------------------------------------

// Can be called by the program
void parse(pickle* vm, object* args, object* env, object* cont, object* fail_cont) {
    DBG("parsing");
    // getarg(vm, args, 0, &string_type, env, fail_cont, vm->wrap_func(PICKLE_INLINE_FUNC {
    //     GOTTEN_ARG(s);
    //     const char* str = (const char*)(s->cells[0].as_chars);
    //     object* result = s->cells[1].as_obj;
    //     const char* message;
    //     bool success = true;
    //     if (result) { // Saved preparse
    //         if (result->schema == &error_type) success = false;
    //         goto done;
    //     }
    //     result = vm->wrap_string("Hello, World! parse result i am."); /* TODO: replace this with the actual parse code */
    //     done:
    //     if (success) vm->set_retval(vm->list(1, result), env, cont, fail_cont);
    //     else {
    //         result = vm->wrap_error(vm->wrap_symbol("SyntaxError"), vm->list(1, vm->wrap_string(message), result), cont);
    //         vm->set_failure(result, env, cont, fail_cont);
    //     }
    //     s->cells[1].as_obj = result; // Save parse for later if constantly eval'ing string (i.e. a loop)
    // }));
}

static object* get_best_match(pickle* vm, object* ast, object** env) {
    return NULL;
}

// Eval(list) ::= apply_first_pattern(list), then eval(remaining list), else list if no patterns match
void eval(pickle* vm, object* args, object* env, object* cont, object* fail_cont) {
    // object* ast = car(args);
    // // returns Match object: 0=pattern, 1=handler body, 2=match details for splice; and updates env with bindings
    // object* oldenv = env;
    // object* matched_pattern = get_best_match(vm, ast, &env);
    // if (matched_pattern != NULL) {
    //     // do next is run body --> cont=apply match cont-> eval again -> original eval cont
    //     vm->do_later(vm->make_partial(
    //         NULL,//matched_pattern->body(),
    //         NULL,
    //         env,
    //         vm->make_partial(
    //             vm->wrap_func(funcs::splice_match),
    //             vm->list(2, vm->append(ast, NULL), NULL/*matched_pattern->match_info()*/),
    //             oldenv,
    //             vm->make_partial(
    //                 vm->wrap_func(funcs::eval),
    //                 NULL,
    //                 oldenv,
    //                 cont,
    //                 fail_cont
    //             ),
    //             fail_cont
    //         ),
    //         fail_cont
    //     ));
    // } else {
    //     // No matches so return unchanged
    //     vm->set_retval(vm->list(1, ast), env, cont, fail_cont);
    // }
}

void splice_match(pickle* vm, object* args, object* env, object* cont, object* fail_cont) {
    // TODO(sm);
}

// ------------------- Circular-reference-proof object dumper -----------------------
// ---------- (based on https://stackoverflow.com/a/78169673/23626926) --------------

static void make_refs_list(pickle* vm, object* obj, object** alist) {
    again:
    DBG();
    if (obj == NULL || obj->type != &cons_type) return;
    object* entry = assoc(*alist, obj);
    if (entry) {
        cdr(entry) = vm->make_integer(2);
        return;
    }
    vm->push(vm->cons(obj, vm->make_integer(1)), *alist);
    make_refs_list(vm, car(obj), alist);
    obj = cdr(obj);
    goto again;
}

// returns zero if the object doesn't need a #N# marker
// otherwise returns N (negative if not first time)
static int64_t reffed(pickle* vm, object* obj, object* alist, int64_t* counter) {
    object* entry = assoc(alist, obj);
    if (entry) {
        int64_t value = vm->unwrap_integer(cdr(entry));
        if (value < 0) {
            // seen already
            return value;
        }
        if (value > 1) {
            // object with shared structure but no id yet
            // assign id
            int64_t my_id = (*counter)++;
            // store entry
            cdr(entry) = vm->make_integer(-my_id);
            return my_id;
        }
    }
    return 0;
}

static void print_with_refs(pickle* vm, object* obj, object* alist, int64_t* counter) {
    if (obj == NULL) {
        printf("NULL");
        return;
    }
    #define PRINTTYPE(t, f, fmt) else if (obj->type == t) printf(fmt, obj->f)
    PRINTTYPE(&string_type, as_chars, "\"%s\"");
    PRINTTYPE(&symbol_type, as_chars, strchr(obj->as_chars, ' ') ? "#|%s|" : "%s");
    PRINTTYPE(&integer_type, as_big_int, "%" PRId64);
    PRINTTYPE(&float_type, as_double, "%lg");
    PRINTTYPE(&c_function_type, as_ptr, "<function %p>");
    PRINTTYPE(NULL, as_ptr, "<garbage %p>");
    #undef PRINTTYPE
    else if (obj->type != &cons_type) printf("<%s:%p>", obj->type->name, obj->as_ptr);
    else {
        // it's a cons
        // test if it's in the table
        int64_t ref = reffed(vm, obj, alist, counter);
        if (ref < 0) {
            printf("#%" PRId64 "#", -ref);
            return;
        }
        if (ref) {
            printf("#%" PRId64 "=", ref);
        }
        // now print the object
        putchar('(');
        for (;;) {
            print_with_refs(vm, car(obj), alist, counter);
            obj = cdr(obj);
            if (reffed(vm, obj, alist, counter)) break;
            if (obj && obj->type == &cons_type) putchar(' ');
            else break;
        }
        if (obj) {
            printf(" . ");
            print_with_refs(vm, obj, alist, counter);
        }
        putchar(')');
    }
}

void pickle::dump(object* obj) {
    object* alist = NULL;
    int64_t counter = 1;
    make_refs_list(this, obj, &alist);
    print_with_refs(this, obj, alist, &counter);
}

}
