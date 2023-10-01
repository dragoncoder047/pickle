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

location::location(size_t line, size_t col, const char* name)
: line(line),
  col(col),
  name(name != NULL ? strdup(name) : NULL) {}

location(const location* other)
: line(other != NULL ? other->line : 1),
  col(other != NULL ? other->col : 1),
  name(other != NULL && other->name != NULL ? strdup(other->name) : NULL) {}

location::~location() {
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
}

static int cmp_c_function(object* a, object* b); {
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
}

static int cmp_string(object* a, object* b) {
    return strcmp((char*)a->cells[0].as_ptr, (char*)b->cells[0].as_ptr);
}

static void mark_string(object* self) {
    self->cells[1].as_obj->mark();
}

static void del_string(object* self) {
    free(self->cells[0].as_ptr);
    delete self->cells;
}

const object_schema metadata_type("object_metadata", init_metadata, NULL, mark_metadata, finalize_metadata);
const object_schema cons_type("cons", tinobsy::schema_functions::init_cons, cmp_c_function, tinobsy::schema_functions::mark_cons, tinobsy::schema_functions::finalize_cons);
const object_schema partial_type("function_partial", init_function_partial, NULL, NULL, tinobsy::schema_functions::finalize_cons);
const object_schema c_function_type("c_function", init_c_function, NULL, NULL, NULL);
const object_schema string_type("string", init_string, cmp_string, mark_string, del_string);

void pickle::cons_list(size_t len, ...) {
    va_list args;
    va_start(args, len);
    object* head;
    object* tail;
    for (size_t i = 0; i < len; i++) {
        object* elem = va_arg(args, object*);
        object* pair = this->allocate(&cons_type, elem, NULL);
        if (i == 0) head = tail = pair;
        else cdr(tail) = pair, tail = pair;
    }
    va_end(args);
    return head;
}

void pickle::append(object* l1, object* l2) {
    object* head;
    object* tail;
    // Clone l1
    for (object* c1 = l1; c1 != NULL; c1 = cdr(c1)) {
        object* elem = car(c1);
        object* pair = this->allocate(&cons_type, elem, NULL);
        if (i == 0) head = tail = pair;
        else cdr(tail) = pair, tail = pair;
    }
    // Point to l2
    cdr(tail) = l2;
    return head;
}

void pickle::set_retval(object* args, object* env, object* cont, object* fail_cont) {
    if (cont == NULL) return; // No next continuation -> drop the result
    object* thunk = this->allocate(&partial_type, cont->cells[0].as_obj, this->append(cont->cells[1].as_obj, args), env, cont->cells[3].as_obj, fail_cont);
    this->do_later(thunk);
}

void pickle::set_failure(object* type, object* details, object* env, object* cont, object* fail_cont) {
    if (fail_cont == NULL) return; // No failure continuation -> ignore the error
    object* ll = this->cons_list(3, type, details, cont);
    object* thunk = this->allocate(&partial_type, failure->cells[0].as_obj, this->append(failure->cells[1].as_obj, args), env, failure->cells[3].as_obj, failure->cells[4].as_obj);
    this->do_later(thunk);
}

void pickle::do_later(object* thunk) {
    object* cell = this->allocate(&cons_type, thunk, NULL);
    cdr(this->queue_tail) = cell;
    this->queue_tail = cell;
    if (this->queue_head == NULL) this->queue_head = cell;
}

void pickle::do_next(object* thunk) {
    this->queue_head = this->allocate(&cons_type, thunk, this->queue_head);
}

void pickle::run_next_thunk() {
    object* thunk = car(this->queue_head);
    this->queue_head = cdr(this->queue_head);
    object* func = thunk->cells[0].as_obj;
    if (func->type == &function_type) {
        func->cells[0].as_func_ptr(
            this,
            thunk->cells[1].as_obj,
            thunk->cells[2].as_obj,
            thunk->cells[3].as_obj,
            thunk->cells[4].as_obj);
    } else {
        object* current_cont = this->allocate(
            &partial_type,
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

void pickle::wrap_func(func_ptr f) {
    return this->allocate(&c_function_type, f);
}

void pickle::wrap_string(const char* s) {
    object* s = this->allocate(&string_type, s);
    if (s->cells[1].as_obj == NULL) // Not already pre-parsed
        this->do_later(this->allocate(
            &partial_type,
            this->wrap_func(funcs::parse),
            this->cons_list(1, s),
            NULL,
            NULL,
            NULL
        ));
    return s;
}

// Can be called by the program
void funcs::parse(pickle* runner, object* args, object* env, object* cont, object* fail_cont) {
    object* s = car(args);
    const char* str = (const char*)(s->cells[0].as_chars);
    object* result = s->cells[1].as_obj;
    if (result != NULL) goto success; // Saved preparse
    // insert magic parse here
    success:
    runner->set_retval(runner->cons_list(1, result), env, cont, fail_cont);
    s->cells[1].as_obj = result; // Save parse for later if constantly reparsing string (i.e. a loop)
    return;
    failure:
    runner->set_failure(runner->syntax_error_symbol, runner->cons_list(1, result), env, cont, fail_cont);
    // TODO: copy error as cached parse result
}


// Eval(list) ::= apply_first_pattern(list), then eval(remaining list), else list if no patterns match
void funcs::eval(pickle* runner, object* args, object* env, object* cont, object* fail_cont) {
    object* ast = car(args);
    // returns Match object: 0=pattern, 1=handler body, 2=match details for splice; and updates env with bindings
    object* oldenv = env;
    object* matched_pattern = get_best_match(runner, ast, &env);
    if (matched_pattern != NULL) {
        // do next is run body --> cont=apply match cont-> eval again -> original eval cont
        runner->do_later(runner->allocate(
            &partial_type,
            matched_pattern->body(),
            NULL,
            env,
            runner->allocate(
                &partial_type,
                runner->wrap_func(funcs::splice_match),
                runner->cons_list(2, runner->append(ast, NULL), matched_pattern->match_info()),
                oldenv,
                runner->allocate(
                    &partial_type,
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
}


}
