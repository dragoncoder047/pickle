## Comments

```py
## This is a line comment (2 hashes + space)

### This is a
block comment  (3+ hashes + space) ###

### This is a #### nested ### block ### #### comment ###
```

## Blocks

```py
## This is a Python style block and the preferred way to make blocks
block header
    block body

## There are curly blocks for those that like them
block header { block body }
```

## Patterns

```py
pattern (bar (the):? [x is Foo]) precedence?
    ## defines a pattern
    ## this will match "bar FooInstance" and "bar the FooInstance" but not "bar the BazInstance" unless Baz inherits from Foo
    doSomethingWith x
    nothing ## implicit return
    return nothing ## explicit return

    ## repetition
    (x):[(N) (N) (B)?] ## repeat min to max, greedy
        ## shorthands
        (x):? => (x):[0 1]
        (x):* => (x):[0 inf]
        (x):+ => (x):[1 inf]
        (x):?? => (x):[0 1 lazy]
        (x):*? => (x):[0 inf lazy]
        (x):+? => (x):[1 inf lazy]

    (x)/(y)/(z) ## alternation

    ## capturing
    [x is (Foo)]
        ## --> captures x if it is an instance of Foo
        ## or if Foo is callable and returns true when given x

    [is Space] ## --> captures but doesn't bind
```

## Functions

```py
fn functionName param param param
    ## implicit return
    nothing

fn functionName (param is Foo) (param or default)
    ## is -> type restrictions: if the param does not match Foo it will throw a TypeError immediately
    ## or -> default value if not provided
```

## Other built-in functions

```py
## Import a module
import [modname is String | Symbol] for [exports is Record] ## like Python "from foo import bar, baz"
import [modname is String | Symbol] as [name is Symbol] ## like Python "import foo as bar"
import [modname is String | Symbol] ## like Python "import foo"

## Control flow

while [condition [is Object]:+][is Space][body is Block]
if [condition [is Object]:+][is Space][body is Block]
synchronized [mutex [is Object]:+][is Space][body is Block]
```
