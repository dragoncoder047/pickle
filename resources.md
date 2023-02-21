PICKLE programming language

`.pik` file extension

* https://en.wikipedia.org/wiki/Prototype-based_programming
* https://github.com/dragoncoder047/lilduino/blob/master/src/lil.h
* https://github.com/dragoncoder047/lilduino/blob/master/src/lil.c
* https://github.com/dragoncoder047/tehssl/blob/main/tehssl.cpp
* https://developer.mozilla.org/en-US/docs/Web/JavaScript/Inheritance_and_the_prototype_chain
* https://docs.python.org/3/reference/datamodel.html

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
    object <|-- undefined_type
    object <|-- unimplemented_type
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
```

| `__less__` | `__equal__` || `<` | `>` | `<=` | `>=` | `==` | `!=` |
|:----------:|:-----------:|:-:|:-:|:--:|:----:|:----:|:----:|:----:|
| F | F || F | True | F | True | F | True |
| F | True || F | F | True | True | True | F |
| True | F || True | F | True | F | F | True |
| True | True || True | F | True | True | True | F | 
