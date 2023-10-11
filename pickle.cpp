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

location::location() {
    DBG("location::location() default: <anonymous>:1:1");
}

location::location(size_t line, size_t col, const char* name)
: line(line),
  col(col),
  name(name != NULL ? strdup(name) : NULL) {
    DBG("location::location() from parameters: %s:%zu:%zu", name, line, col);
}

location::location(const location* other)
: line(other != NULL ? other->line : 1),
  col(other != NULL ? other->col : 1),
  name(other != NULL && other->name != NULL ? strdup(other->name) : NULL) {
    DBG("location::location() from existing location: %s:%zu:%zu", this->name, this->line, this->col);
}

location::~location() {
    DBG("location::~location()");
    free(this->name);
}

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
    DBG("Function is eval(): %s", self->as_ptr == funcs::eval ? "true" : "false");
    DBG("Function is parse(): %s", self->as_ptr == funcs::parse ? "true" : "false");
}

static int cmp_c_function(object* a, object* b) {
    return (uintptr_t)a->as_ptr - (uintptr_t)b->as_ptr;
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

static void init_error(object* self, va_list args) {
    DBG("Creating an error");
    self->cells = new cell[3];
    self->cells[0].as_obj = va_arg(args, object*);
    self->cells[1].as_obj = va_arg(args, object*);
    self->cells[2].as_obj = va_arg(args, object*);
}

static void mark_error(object* self) {
    for (int i = 0; i < 3; i++) self->cells[i].as_obj->mark();
}

const object_schema metadata_type("object_metadata", init_metadata, NULL, mark_metadata, finalize_metadata);
const object_schema cons_type("cons", tinobsy::schema_functions::init_cons, cmp_c_function, tinobsy::schema_functions::mark_cons, tinobsy::schema_functions::finalize_cons);
const object_schema partial_type("function_partial", init_function_partial, NULL, NULL, tinobsy::schema_functions::finalize_cons);
const object_schema c_function_type("c_function", init_c_function, NULL, NULL, NULL);
const object_schema string_type("string", init_string, cmp_string, mark_string, del_string);
const object_schema symbol_type("symbol", tinobsy::schema_functions::init_str, tinobsy::schema_functions::cmp_str, NULL, tinobsy::schema_functions::finalize_str);
const object_schema error_type("error", init_error, NULL, mark_error, tinobsy::schema_functions::finalize_cons);

object* pickle::cons_list(size_t len, ...) {
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
            this->cons_list(1, func), // args is ignored because they should already be added to env
            thunk->cells[2].as_obj,
            thunk->cells[3].as_obj,
            thunk->cells[4].as_obj);
        this->do_later(current_cont);
    }
}

void pickle::mark_globals() {
    this->queue_head->mark();
    this->queue_tail->mark(); // in case queue gets detached
}

object* pickle::wrap_string(const char* chs) {
    object* s = this->allocate(&string_type, chs);
    if (s->cells[1].as_obj == NULL) {// Interned but not already pre-parsed
        DBG("Starting task to parse string %s", chs);
        this->do_later(this->make_partial(
            this->wrap_func(funcs::parse),
            this->cons_list(1, s),
            NULL,
            NULL,
            NULL
        ));
    }
    return s;
}

// Can be called by the program
void funcs::parse(pickle* runner, object* args, object* env, object* cont, object* fail_cont) {
    DBG("parsing");
    object* s = car(args);
    const char* str = (const char*)(s->cells[0].as_chars);
    object* result = s->cells[1].as_obj;
    if (result != NULL) { // Saved preparse
        if (result->schema == &error_type) goto failure;
        else goto success;
    }
    TODO;
    // result = runner->make_error(runner->wrap_symbol("SyntaxError"), runner->cons_list(1, result), cont)
    success:
    runner->set_retval(runner->cons_list(1, result), env, cont, fail_cont);
    s->cells[1].as_obj = result; // Save parse for later if constantly reparsing string (i.e. a loop)
    return;
    failure:
    runner->set_failure(result, env, cont, fail_cont);
    // TODO: copy error as cached parse result
}

static object* get_best_match(pickle* runner, object* ast, object** env) {
    TODO;
    return NULL;
}

// Eval(list) ::= apply_first_pattern(list), then eval(remaining list), else list if no patterns match
void funcs::eval(pickle* runner, object* args, object* env, object* cont, object* fail_cont) {
    object* ast = car(args);
    // returns Match object: 0=pattern, 1=handler body, 2=match details for splice; and updates env with bindings
    object* oldenv = env;
    object* matched_pattern = get_best_match(runner, ast, &env);
    if (matched_pattern != NULL) {
        // do next is run body --> cont=apply match cont-> eval again -> original eval cont
        runner->do_later(runner->make_partial(
            NULL,//matched_pattern->body(),
            NULL,
            env,
            runner->make_partial(
                runner->wrap_func(funcs::splice_match),
                runner->cons_list(2, runner->append(ast, NULL), NULL/*matched_pattern->match_info()*/),
                oldenv,
                runner->make_partial(
                    runner->wrap_func(funcs::eval),
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
        runner->set_retval(runner->cons_list(1, ast), env, cont, fail_cont);
    }
}

void funcs::splice_match(pickle* runner, object* args, object* env, object* cont, object* fail_cont) {
    TODO;
}


}
