#include "pickle.hpp"
#include <cerrno>

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

template <int n> void initmulti(object* self, va_list args) {
    self->cells = new cell[n];
    DBG("creating %s (%i cells)", self->schema->name, n);
    for (int i = 0; i < n; i++) self->cells[i].as_obj = va_arg(args, object*);

}

template <int n> void markmulti(object* self) {
    for (int i = 0; i < n; i++) self->cells[i].as_obj->mark();
}

#define freemulti tinobsy::schema_functions::finalize_cons

static void init_c_function(object* self, va_list args) {
    self->as_ptr = (void*)va_arg(args, func_ptr);
    DBG("Function is eval(): %s", self->as_ptr == funcs::eval ? "true" : "false");
    DBG("Function is parse(): %s", self->as_ptr == funcs::parse ? "true" : "false");
}

static int cmp_c_function(object* a, object* b) {
    return (uintptr_t)a->as_ptr - (uintptr_t)b->as_ptr;
}

static void init_string(object* self, va_list args) {
    self->cells = new cell[2];
    self->cells[0].as_ptr = (void*)strdup(va_arg(args, char*));
    DBG("init_string: %s", (char*)self->cells[0].as_ptr);
    self->cells[1].as_ptr = NULL; // preparsed holder
}

static int cmp_string(object* a, object* b) {
    return strcmp((char*)a->cells[0].as_ptr, (char*)b->cells[0].as_ptr);
}

static void mark_string(object* self) {
    self->cells[1].as_obj->mark();
}

static void del_string(object* self) {
    free(self->cells[0].as_ptr);
    delete[] self->cells;
}

static void init_int(object* self, va_list args) {
    self->as_big_int = va_arg(args, int64_t);
}

static int cmp_int(object* a, object* b) {
    return a->as_big_int - b->as_big_int;
}

static void init_float(object* self, va_list args) {
    self->as_double = va_arg(args, double);
}

static int cmp_float(object* a, object* b) {
    return (int)(a->as_double - b->as_double);
}

// ------------------------ core types -----------------
// these will later be swapped for actual objects

// metadata = line, column, file, prototypes 
const object_schema metadata_type("object_metadata", initmulti<4>, NULL, markmulti<4>, freemulti);
// cons = car, cdr
const object_schema cons_type("cons", tinobsy::schema_functions::init_cons, NULL, tinobsy::schema_functions::mark_cons, freemulti);
// partial = function, args, env, then, catch
const object_schema partial_type("function_partial", initmulti<5>, NULL, markmulti<5>, freemulti);
// error = type, message, detail, then
const object_schema error_type("error", initmulti<4>, NULL, markmulti<4>, freemulti);
// list = items
const object_schema list_type("list", initmulti<1>, NULL, markmulti<1>, freemulti);
// --------- primitive/ish types ---------------
const object_schema string_type("string", init_string, cmp_string, mark_string, del_string);
const object_schema symbol_type("symbol", tinobsy::schema_functions::init_str, tinobsy::schema_functions::cmp_str, NULL, tinobsy::schema_functions::finalize_str);
const object_schema c_function_type("c_function", init_c_function, cmp_c_function, NULL, NULL);
const object_schema integer_type("int", init_int, cmp_int, NULL, NULL);
const object_schema float_type("float", init_float, cmp_float, NULL, NULL);

object* pickle::list(size_t len, ...) {
    va_list args;
    va_start(args, len);
    object* head;
    object* tail;
    for (size_t i = 0; i < len; i++) {
        object* elem = va_arg(args, object*);
        object* pair = this->cons(elem, NULL);
        if (i == 0) head = tail = pair;
        else cdr(tail) = pair, tail = pair;
    }
    va_end(args);
    return head;
}

object* pickle::append(object* l1, object* l2) {
    object* head;
    object* tail;
    // Clone l1
    size_t i = 0;
    for (object* c1 = l1; c1 != NULL; c1 = cdr(c1), i++) {
        object* elem = car(c1);
        object* pair = this->cons(elem, NULL);
        if (i == 0) head = tail = pair;
        else cdr(tail) = pair, tail = pair;
    }
    // Point to l2
    cdr(tail) = l2;
    return head;
}

void pickle::set_retval(object* args, object* env, object* cont, object* fail_cont) {
    if (cont == NULL) return; // No next continuation -> drop the result
    if (cont->schema == &c_function_type) {
        // stupid waste of an object
        cont = this->make_partial(cont, args, env, NULL, fail_cont);
    }
    object* thunk = this->make_partial(cont->cells[0].as_obj, this->append(cont->cells[1].as_obj, args), env, cont->cells[3].as_obj, fail_cont);
    this->do_later(thunk);
}

void pickle::set_failure(object* err, object* env, object* cont, object* fail_cont) {
    if (fail_cont == NULL) return; // No failure continuation -> ignore the error
    object* thunk = this->make_partial(fail_cont->cells[0].as_obj, this->append(fail_cont->cells[1].as_obj, this->cons(err, NULL)), env, fail_cont->cells[3].as_obj, fail_cont->cells[4].as_obj);
    this->do_later(thunk);
}

void pickle::do_later(object* thunk) {
    DBG("do_later: Adding cons to tail");
    object* cell = this->cons(thunk, NULL);
    if (this->queue_tail != NULL) cdr(this->queue_tail) = cell;
    this->queue_tail = cell;
    if (this->queue_head == NULL) this->queue_head = cell;
}

void pickle::do_next(object* thunk) {
    DBG("do_next");
    this->queue_head = this->cons(thunk, this->queue_head);
}

void pickle::run_next_thunk() {
    DBG("run_next_thunk");
    if (this->queue_head == NULL) return;
    object* thunk = car(this->queue_head);
    DBG("Have thunk");
    this->queue_head = cdr(this->queue_head);
    if (this->queue_head == NULL) this->queue_tail = NULL;
    object* func = thunk->cells[0].as_obj;
    DBG("Have func");
    if (func->schema == &c_function_type) {
        DBG("Native function");
        ((func_ptr)(func->as_ptr))(
            this,
            thunk->cells[1].as_obj,
            thunk->cells[2].as_obj,
            thunk->cells[3].as_obj,
            thunk->cells[4].as_obj);
    } else {
        DBG("Data function");
        object* current_cont = this->make_partial(
            this->wrap_func(funcs::eval),
            this->list(1, func), // args is ignored because they should already be added to env
            thunk->cells[2].as_obj,
            thunk->cells[3].as_obj,
            thunk->cells[4].as_obj);
        this->do_later(current_cont);
    }
}

void pickle::mark_globals() {
    this->queue_head->mark();
    this->queue_tail->mark(); // in case queue gets detached
    this->globals->mark();
}

void getarg(pickle* vm, object* args, size_t nth, const object_schema* type, object* env, object* fail, object* then) {
    auto oa = args;
    for (size_t i = 0; i < nth; i++) {
        if (cdr(args) == NULL) {
            vm->set_failure(vm->wrap_error(vm->wrap_symbol("ValueError"), vm->list(2, oa, vm->wrap_integer(nth)), then), env, then, fail);
            return;
        }
        args = cdr(args);
    }
    auto val = car(args);
    if (val == NULL || (val->schema != type)) 
        vm->set_failure(vm->wrap_error(vm->wrap_symbol("TypeError"), vm->list(3, oa, vm->wrap_integer(nth), val), then), env, then, fail);
    else 
        vm->set_retval(vm->list(2, car(args), oa), env, then, fail);
}

//--------------- PARSER --------------------------------------

// Can be called by the program
void funcs::parse(pickle* vm, object* args, object* env, object* cont, object* fail_cont) {
    DBG("parsing");
    getarg(vm, args, 0, &string_type, env, fail_cont, vm->wrap_func(PICKLE_INLINE_FUNC {
        GOTTEN_ARG(s);
        const char* str = (const char*)(s->cells[0].as_chars);
        object* result = s->cells[1].as_obj;
        const char* message;
        bool success = true;
        if (result) { // Saved preparse
            if (result->schema == &error_type) success = false;
            goto done;
        }
        result = vm->wrap_string("Hello, World! parse result i am.");
        done:
        if (success) vm->set_retval(vm->list(1, result), env, cont, fail_cont);
        else {
            result = vm->wrap_error(vm->wrap_symbol("SyntaxError"), vm->list(1, vm->wrap_string(message), result), cont);
            vm->set_failure(result, env, cont, fail_cont);
        }
        s->cells[1].as_obj = result; // Save parse for later if constantly eval'ing string (i.e. a loop)
    }));
}

static object* get_best_match(pickle* vm, object* ast, object** env) {
    TODO(gbm);
    return NULL;
}

// Eval(list) ::= apply_first_pattern(list), then eval(remaining list), else list if no patterns match
void funcs::eval(pickle* vm, object* args, object* env, object* cont, object* fail_cont) {
    object* ast = car(args);
    // returns Match object: 0=pattern, 1=handler body, 2=match details for splice; and updates env with bindings
    object* oldenv = env;
    object* matched_pattern = get_best_match(vm, ast, &env);
    if (matched_pattern != NULL) {
        // do next is run body --> cont=apply match cont-> eval again -> original eval cont
        vm->do_later(vm->make_partial(
            NULL,//matched_pattern->body(),
            NULL,
            env,
            vm->make_partial(
                vm->wrap_func(funcs::splice_match),
                vm->list(2, vm->append(ast, NULL), NULL/*matched_pattern->match_info()*/),
                oldenv,
                vm->make_partial(
                    vm->wrap_func(funcs::eval),
                    NULL,
                    oldenv,
                    cont,
                    fail_cont
                ),
                fail_cont
            ),
            fail_cont
        ));
    } else {
        // No matches so return unchanged
        vm->set_retval(vm->list(1, ast), env, cont, fail_cont);
    }
}

void funcs::splice_match(pickle* vm, object* args, object* env, object* cont, object* fail_cont) {
    TODO(sm);
}


}
