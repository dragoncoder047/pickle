#include "pickle.hpp"
#include <errno.h>
#include <inttypes.h>

namespace pickle {

static void free_payload(object* o) { free(o->as_ptr); }
static object* mark_car_only(tinobsy::vm* _, object* o) { return car(o); }

// ------------------------ core types -----------------
// these will later be swapped for actual objects

// cons = car, cdr
const object_type cons_type("cons", tinobsy::markcons, NULL, NULL);
const object_type obj_type("object", tinobsy::markcons, NULL, NULL);
// --------- primitive/ish types ---------------
const object_type string_type("string", mark_car_only, free_payload, NULL);
const object_type symbol_type("symbol", mark_car_only, free_payload, NULL);
const object_type c_function_type("c_function", mark_car_only, NULL, NULL);
const object_type integer_type("int", mark_car_only, NULL, NULL);
const object_type float_type("float", mark_car_only, NULL, NULL);
const object_type* primitives[] = { &string_type, &symbol_type, &c_function_type, &integer_type, &float_type, NULL };

// ----------------- misc init functions ---------------------------

void pvm::mark_globals() {
    this->markobject(this->queue);
    this->markobject(this->globals);
    this->markobject(this->function_registry);
}

pvm::pvm() {
    tinobsy::vm();
    srand(time(NULL));
    this->hash_seed = rand();
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

// ---------- STACK MACHINE --------------------------------------------

void pvm::start_thread()  {
    // thread is list of (data stack, next instruction, instruction stack)
    object* new_thread = this->cons(nil, this->cons(nil, nil));
    if (!this->queue) {
        this->queue = this->cons(new_thread, NULL);
        cdr(this->queue) = this->queue;
        return;
    }
    object* last = this->queue;
    // Find last element of queue
    while (cdr(last) != this->queue) last = cdr(last);
    this->push(new_thread, this->queue);
    cdr(last) = this->queue;
}

void pvm::step() {
    next_inst:
    if (!this->queue) return;
    object* next_type = cadr(this->curr_thread());
    object* op = this->pop_inst();
    if (!op) {
        object* last = this->queue;
        if (cdr(last) == last) {
            // last thread and nothing to do
            this->queue = nil;
            return;
        }
        while (cdr(last) != this->queue) last = cdr(last);
        // Drop the empty thread
        this->queue = cdr(last) = cdr(this->queue);
        goto next_inst;
    }
    object* type = car(op);
    if (eqcmp(type, next_type) != 0) goto next_inst;
    object* inst_name = cadr(op);
    object* cookie = cddr(op);
    object* pair = assoc(this->function_registry, inst_name);
    ASSERT(pair, "Unknown instruction %s", this->stringof(inst_name));
    next_type = this->fptr(cdr(pair))(this, cookie, next_type);
    cadr(this->curr_thread()) = next_type;
    this->queue = cdr(this->queue);
}

//--------------- PARSER --------------------------------------

namespace parser {

typedef struct {
    const char* data;
    size_t i;
    size_t len;
} pstate;

#define pos (s->i)
#define advance pos +=
#define next pos++
#define look (s->data[pos])
#define at(z) (&s->data[z])
#define here at(pos)
#define eofp  (pos >= s->len)
#define test(f) (f(look))

static void bufadd(char** b, char c) {
    // super not memory efficient, it reallocs the buffer every time
    char* ob = *b;
    asprintf(b, "%s%c", *b ? *b : "", c);
    free(ob);
}

static void bufcat(char** b, const char* c, int n) {
    char* ob = *b;
    asprintf(b, "%s%.*s", *b ? *b : "", n, c);
    free(ob);
}

static char unescape(char c) {
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

static char escape(char c) {
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

static char revparen(char p) {
    const char* a = "([{}])";
    const char* b = ")]}{[(";
    return b[strchr(a, p) - a];
}

static object* next_token(pvm* vm, pstate* s) {
    char c = look;
    char* b = NULL;
    char* b2 = NULL;
    object* result = nil;
    if (isalpha(c) || c == '_') {
        DBG("symbol");
        size_t p = pos;
        while (!eofp && (test(isalpha) || test(isdigit) || look == '_')) next;
        bufcat(&b, at(p), pos - p);
        result = vm->sym(b);
    }
    else if (isdigit(c)) {
        DBG("number");
        double d; int64_t n;
        int num;
        int ok = sscanf(here, "%lg%n", &d, &num);
        if (ok) result = vm->number(d);
        else {
            ok = sscanf(here, "%" SCNi64 "%n", &n, &num);
            if (ok) result = vm->integer(n);
            else result = nil; // TODO: report error, or parse bigint?
        }
        DBG("ok = %i", ok);
        if (ok) advance num;
    }
    else if (c == '\n' || c == '\r') {
        DBG("newline");
        while (look == '\n' || look == '\r') next;
        result = vm->sym("NEWLINE");
    }
    else if (isspace(c) && c != '\n') {
        DBG("space");
        size_t p = pos;
        while (!eofp && (test(isspace) && look != '\n' && look != '\r')) next;
        bufcat(&b, at(p), pos - p);
        result = vm->sym(b);
    }
    else if (ispunct(c)) {
        DBG("punctuation symbol");
        // must test for other punctuation last to allow other special cases to take precedence
        next;
        bufadd(&b, c);
        result = vm->sym(b);
    }
    else {
        DBG("other crap: %c (%i)", c, (int)c);
        next;
        result = nil;
    }
    done:
    free(b);
    free(b2);
    return result;
}

// Can be called by the program
object* tokenize(pvm* vm, object* cookie, object* inst_type) {
    (void)cookie;
    DBG("tokenizing");
    object* string = vm->pop();
    if (string->type != &string_type) {
        vm->push_data(vm->cons(vm->string("non string to tokenize()"), nil));
        return vm->sym("error");
    }
    const char* str = vm->stringof(string);
    pstate s = { .data = str, .i = 0, .len = strlen(str) };
    object* result = nil;
    object** tail = &result;
    do {
        object* item = next_token(vm, &s);
        DBG("Got token.");
        *tail = vm->cons(item, nil);
        tail = &cdr(*tail);
    } while (s.i < s.len);
    vm->push_data(result);
    return nil;
}

#undef pos
#undef advance
#undef next
#undef look
#undef at
#undef here
#undef eofp
#undef test

}


// ------------------------- HASHMAPS (OBJECTS) -------------------------------

namespace hashmap {

// Returns the found node or nil if the hash is not found.
static object* get(pvm* vm, object* map, uint64_t hash) {
    // Each hashmap node is a 4-cons tree ((hash . (key . value)) . (left . right))
    // but this gets printed as ((hash key . value) left . right) when the map is printed as-is
    uint64_t hh = hash;
    DBG("Searching hashmap for hash %" PRId64 " {", hash);
    recurse:
    if (!map) {
        DBG("Node is nil -- not found. }");
        return nil;
    }
    object* hash_pair = car(map);
    if (hash_pair) {
        int64_t this_hash = vm->intof(car(hash_pair));
        DBG("this_hash=%" PRId64, this_hash);
        if (this_hash == hash) {
            DBG("Found matching key for hash %" PRId64 " }", hash);
            return map;
        }
    }
    bool ll = hh & 1;
    object* children = cdr(map);
    if (!children) {
        DBG("Reached node with no children -- Not found. }");
        return nil;
    }
    if (ll) map = car(children);
    else map = cdr(children);
    hh >>= 1;
    DBG("Recursing on %s", ll ? "LEFT" : "RIGHT");
    goto recurse;
}

// Returns the new node, *map is updated to point to the root node if it changed.
static object* set(pvm* vm, object** map, object* key, uint64_t hash, object* val) {
    DBG("Setting hash %" PRId64 " on hashmap. {", hash);
    uint64_t hh = hash;
    object* newnode = nil;
    recurse:
    if (*map == nil) {
        DBG("Tree is terminated -- add new node. }");
        *map = vm->cons(vm->cons(vm->integer(hash), vm->cons(key, val)), nil);
        return *map;
    }
    // Map is not empty at this level. Check to see if it is a free node or the target node.
    object* hash_pair = car(*map);
    bool ll = hh & 1;
    object* children = cdr(*map);
    if (!hash_pair) {
        DBG("Found tombstoned node. Inserting key.");
        car(*map) = vm->cons(vm->integer(hash), vm->cons(key, val));
        newnode = *map;
        if (!children) return newnode; // No children to search and kill
        goto killshadow;
    } else {
        // Check if the hashes match
        int64_t z = vm->intof(car(hash_pair));
        if (z == hash) {
            DBG("Found matching node. Re-setting it. }");
            if (!cdr(hash_pair)) cdr(hash_pair) = vm->cons(nil, nil);
            cadr(hash_pair) = key;
            cddr(hash_pair) = val;
            return *map;
        }
    }
    if (!children) {
        DBG("Reached node with no children cons. Adding children cons.");
        cdr(*map) = children = vm->cons(nil, nil);
    }
    if (ll) map = &car(children);
    else map = &cdr(children);
    hh >>= 1;
    DBG("Recursing on %s", ll ? "LEFT" : "RIGHT");
    goto recurse;
    //-------------
    killshadow:
    DBG("Now searching for shadower nodes in search path and killing them.");
    if (ll) map = &car(children);
    else map = &cdr(children);
    hh >>= 1;
    DBG("Continuing on %s", ll ? "LEFT" : "RIGHT");
    killagain:
    if ((*map) == nil) {
        DBG("Reached end of hash path. Done killing. }");
        return newnode;
    }
    hash_pair = car(*map);
    ll = hh & 1;
    children = cdr(*map);
    if (hash_pair) {
        int64_t bad = vm->intof(car(hash_pair));
        if (bad == hash) {
            DBG("Found shadowing node, killing it.");
            car(*map) = nil;
        }
    }
    if (children == nil) {
        DBG("Reached node with no children. Stopping }");
        return newnode;
    }
    if (ll) map = &car(children);
    else map = &cdr(children);
    hh >>= 1;
    DBG("Shadow recursing on %s", ll ? "LEFT" : "RIGHT");
    goto killagain;
}

}

object* pvm::get_property(object* obj, uint64_t hash, bool recurse) {
    // Nil has no properties
    if (!obj) return nil;
    if (recurse) {
        DBG("Inheritance requested get_property() {");
        // Try to find it directly.
        object* val = this->get_property(obj, hash, false);
        if (val) {
            DBG("Own property. }");
            return val;
        }
        // Not found, traverse prototypes list.
        for (object* p = car(obj); p; p = cdr(p)) {
            val = this->get_property(car(p), hash, true);
            if (val) {
                DBG("Parent property. }");
                return val;
            }
        }
        DBG("Property not found in inheritance tree. }");
        return nil;
    }
    // Check if it is an object-object (primitives have no own properties)
    if (obj->type != &obj_type) return nil;
    // Search the hashmap.
    object* hashmap = cdr(obj);
    object* node = hashmap::get(this, hashmap, hash);
    if (node) return cddar(node);
    return nil;
}

bool pvm::set_property(object* obj, object* key, uint64_t hash, object* value) {
    // Nil has no properties
    if (!obj) return false;
    // Check if it is an object-object (primitives have no own properties)
    if (obj->type != &obj_type) return false;
    hashmap::set(this, &cdr(obj), key, hash, value);
    return true;
}

bool pvm::remove_property(object* obj, uint64_t hash) {
    // Nil has no properties
    if (!obj) return false;
    // Check if it is an object-object (primitives have no own properties)
    if (obj->type != &obj_type) return false;
    bool had = hashmap::get(this, cdr(obj), hash) != nil;
    // Try to set the node to nil, which will kill the shadow references
    object* node = hashmap::set(this, &cdr(obj), nil, hash, nil);
    // Then kill this node too
    ASSERT(node, "hashmap_set() failed");
    car(node) = nil;
    return had;
}

// ------------------ PATTERN MATCHING -----------------------------

static object* get_best_match(pvm* vm, object* ast, object** env) {
    return nil;
}

// Eval(list) ::= apply_first_pattern(list), then eval(remaining list), else list if no patterns match
object* eval(pvm* vm, object* cookie, object* inst_type) {
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
    return nil;
}

object* splice_match(pvm* vm, object* cookie, object* inst_type) {
    // TODO(sm);
    return nil;
}

// ------------------- Circular-reference-proof object dumper -----------------------
// ---------- (based on https://stackoverflow.com/a/78169673/23626926) --------------

namespace dumper {

static void make_refs_list(pvm* vm, object* obj, object** alist) {
    again:
    if (obj == NULL || (obj->type != &cons_type && obj->type != &obj_type)) return;
    object* entry = assoc(*alist, obj);
    if (entry) {
        cdr(entry) = vm->integer(2);
        return;
    }
    vm->push(vm->cons(obj, vm->integer(1)), *alist);
    if (obj->type != &obj_type) make_refs_list(vm, car(obj), alist); // hashmaps are guaranteed non disjoint, i guess
    obj = cdr(obj);
    goto again;
}

// returns zero if the object doesn't need a #N# marker
// otherwise returns N (negative if not first time)
static int64_t reffed(pvm* vm, object* obj, object* alist, int64_t* counter) {
    object* entry = assoc(alist, obj);
    if (entry) {
        int64_t value = vm->intof(cdr(entry));
        if (value < 0) {
            // seen already
            return value;
        }
        if (value > 1) {
            // object with shared structure but no id yet
            // assign id
            int64_t my_id = (*counter)++;
            // store entry
            cdr(entry) = vm->integer(-my_id);
            return my_id;
        }
    }
    return 0;
}

static void print_with_refs(pvm*, object*, object*, int64_t*);

static void print_hashmap(pvm* vm, object* node, object* alist, int64_t* counter) {
    recur:
    if (node) {
        if (car(node)) {
            object* hinfo = car(node);
            print_with_refs(vm, cadr(hinfo), alist, counter);
            printf(" -> ");
            print_with_refs(vm, cddr(hinfo), alist, counter);
            printf(" ;[hash=%" PRId64 "] ", vm->intof(car(hinfo)));
        }
        if (!cdr(node)) return;
        print_hashmap(vm, cadr(node), alist, counter);
        node = cddr(node);
        goto recur;
    }
}

static void print_with_refs(pvm* vm, object* obj, object* alist, int64_t* counter) {
    if (obj == nil) {
        printf("NIL");
        return;
    }
    // test if it's in the table
    int64_t ref = reffed(vm, obj, alist, counter);
    if (ref < 0) {
        printf("#%" PRId64 "#", -ref);
        return;
    }
    if (ref) {
        printf("#%" PRId64 "=", ref);
    }
    #define PRINTTYPE(t, f, fmt) else if (obj->type == t) printf(fmt, obj->f)
    if (obj->type == &string_type) {
        putchar('"');
        for (char* c = obj->as_chars; *c; c++) {
            char e = parser::escape(*c);
            if (e != *c) {
                putchar('\\');
                putchar(e);
            }
            else putchar(*c);
        }
        putchar('"');
    }
    PRINTTYPE(&symbol_type, as_chars, strpbrk(obj->as_chars, "(){}[] ") ? "#|%s|" : "'%s");
    PRINTTYPE(&integer_type, as_big_int, "%" PRId64);
    PRINTTYPE(&float_type, as_double, "%lg");
    PRINTTYPE(&c_function_type, as_ptr, "<function %p>");
    PRINTTYPE(NULL, as_ptr, "<garbage %p>");
    #undef PRINTTYPE
    else if (obj->type == &cons_type) {
        // it's a cons and unreffed
        // now print the object
        putchar('(');
        for (;;) {
            print_with_refs(vm, car(obj), alist, counter);
            obj = cdr(obj);
            int64_t ref = reffed(vm, obj, alist, counter);
            if (ref) {
                if (ref > 0) {
                    // reset the ref so it will print properly
                    cdr(assoc(alist, obj)) = vm->integer(ref);
                    (*counter)--;
                }
                break;
            }
            if (obj && obj->type == &cons_type) putchar(' ');
            else break;
        }
        if (obj) {
            printf(" . ");
            print_with_refs(vm, obj, alist, counter);
        }
        putchar(')');
    }
    else if (obj->type == &obj_type) {
        // Try to find the class name
        // TODO: String/symbol/int static hash.
        const char* nm = "object";
        // if (car(obj) && car(car(obj))) {
        //     object* super = car(car(obj));
        //     object* name = vm->get_property(super, vm->static_hash(vm->string("__name__")));
        //     if (name->type == &symbol_type) nm = vm->stringof(name);
        // }
        printf("%s{ ", nm);
        print_hashmap(vm, cdr(obj), alist, counter);
        putchar('}');
    }
    else printf("<%s:%p>", obj->type->name, obj->as_ptr);
}

}

void pvm::dump(object* obj) {
    object* alist = NULL;
    int64_t counter = 1;
    dumper::make_refs_list(this, obj, &alist);
    dumper::print_with_refs(this, obj, alist, &counter);
}


size_t pvm::gc() {
    DBG("TODO: garbage collect all of the unused hashmap nodes");
    return tinobsy::vm::gc();
}

}
