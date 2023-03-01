PICKLE programming language

`.pik` file extension

* <https://en.wikipedia.org/wiki/Prototype-based_programming>
* <https://github.com/dragoncoder047/lilduino/blob/master/src/lil.h>
* <https://github.com/dragoncoder047/lilduino/blob/master/src/lil.c>
* <https://github.com/dragoncoder047/tehssl/blob/main/tehssl.cpp>
* <https://developer.mozilla.org/en-US/docs/Web/JavaScript/Inheritance_and_the_prototype_chain>
* <https://docs.python.org/3/reference/datamodel.html>

```mermaid
classDiagram
    direction LR
    class object {
        type_t type
        object gc_next
        size_t refcount
        flags_t global_flags
        flags_t object_flags
        object* prototypes
        size_t num_prototypes
        size_t alloc_prototypes
        object __stringify__()
        object __call__()
        hashmap_t properties
    }
    class number {
        object __plus__()
        object __minus__()
        object __times__()
        object __divide__()
        object __pow__()
        object __bitand__()
        object __bitor__()
        object __bitxor__()
        object __shr__()
        object __shl__()
        object __less__()
        object __equal__()
    }
    object <|-- number
    class int {
        int64_t number
    }
    class float {
        double number
    }
    class complex {
        (overrides number methods)
        float real
        float imag
    }
    class bool {
        <<enumeration>>
        True
        False
    }
    number <|-- int
    number <|-- float
    number <|-- complex
    int <|-- bool
    object <|-- indexable
    class indexable {
        object __getitem__()*
        object __plus__()* : explicit concatenation
        object __concat__()* : Tcl-style concatenation
        object __bitand__()* : set intersection
        object __bitor__()* : set union
        object __minus__()* : set subtraction
        object __has__()* : 'in'/'has' operator
        object length()*
    }
    object <|-- string
    class string {
        char* str
        size_t length
        size_t capacity
        object __less__() : dictionary ordering
        object __equal__()
    }
    indexable <|-- list
    class list {
        object* items
        size_t length
        size_t capacity
    }
    indexable <|-- dict
    class dict {
        hashmap_t items
    }
    object <|-- callable
    class callable {
        object __call__()*
    }
    callable <|-- function
    class function {
        block body
        char** argn
        size_t argc
        scope closure
        object __pipe__() : the |> pipe operator
    }
    callable <|-- type
    class type {
        The base of user classes
        Referenced as prototype of objects
    }
    object <|-- error
    class error {
        char* message
    }    
    %% ----------------------- Internal types ------------
    string <|-- unspecialized_string
    object <|-- scope
    class scope {
        object result_value
        resultcode_t code;
        (parent scope is prototype of object)
        (upeval scope is __upeval__ property)
    }
    object <|-- block
    class block {
        size_t length
        statement* contents
    }
    block <|-- statement
    class statement {
        object* words
        size_t wordcount
        size_t wordcap
    }
    statement <|-- dollarexpr
    statement <|-- implicit_concat
    object <|-- NIL
```

| `__less__` | `__equal__` || `<` | `>` | `<=` | `>=` | `==` | `!=` |
|:----------:|:-----------:|:-:|:-:|:--:|:----:|:----:|:----:|:----:|
| F | F || F | True | F | True | F | True |
| F | True || F | F | True | True | True | F |
| True | F || True | F | True | F | F | True |
| True | True || True | F | True | True | True | F |

Special vars

* `$?` == result of last expression
* `$@` == args
* `$#` == self
* `$^` == target var in `|>` expression
* `$0`... = lambda args

```js
/\$([?@#^]|[0-9]+|[A-Za-z_][A-Za-z0-9]*)/ // Variable
/\.([A-Za-z_][A-Za-z0-9]*)/ // Property
/\[expr\]/ // Getitem after a variable -- ONLY valid immediatley after a variable with NO whitespace
// These are processed by the `set` command not the main Pickle parser
```

Syntax

* `# ...` == line comments
* `### ... ###` == block comments
* `{...}` == literal string (could be code)
* `"..."` == literal string (could be code)
* `'...'` == literal string (could be code)
* `[...]` == list
* `(...)` == parens
* Newline or `;` == end of command

`foo bar baz` is the same as `foo.bar baz` if `foo` can't be called; `foo.bar.baz` if `foo.bar` can't be called; and a type error any other way (if some property in the chain doesn't exist, for example)

Literal for NULL is `null` or `pass`

* each item on the line is literalized at compile time to either number or string
* the args of each line are evaluated in an expresssion, recursively merging (thing op thing) to result of op
* first object is called with second object, unless in list syntax, where it is just returned as a new list

Example:

```tcl
print 3 + 4 ;#>> print 7
print foo bar + baz ;#>> print foo barbaz
print foo + bar bax * 3 ;#>> print foobar baxbaxbax
print * 3 ;#>> syntax error
```
