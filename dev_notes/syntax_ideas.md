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
    [x (Foo)] ## captures what Foo matches

    [x is/matches (Foo)]
        ## shorthand for [x [is/matches Foo]]
        ## --> captures x if it is an instance of Foo (is)
        ##             or if calling Foo with x returns true (matches)

    [is (Foo)] ## --> matches but doesn't bind

    [is Space] ## special because implicit spaces don't match newlines, this explicit space does
```

## Functions

```py
fn functionName param param param
    ## implicit return
    nothing

fn functionName (param is/matches Foo) (param or default)
    ## is -> type restrictions: if the param does not match Foo it will throw a TypeError immediately
    ## or -> default value if not provided
```

## Other built-in constructs

```py
## Import a module
import [modname is String | Symbol] for [exports is Record] ## like Python "from foo import bar, baz"
import [modname is String | Symbol] for [name is Symbol] ## like Python "import foo as bar"
import [modname is String | Symbol] ## like Python "import foo"

## Control flow
while [condition any:+][is Space][body is Block]
if [condition any:+][is Space][body is Block]

## Dynamic-wind but like Python context manager
with [context-manager is ContextManager][is Space][body is Block]

## Synchronization
synchronized [mutex][is Space][body is Block]

## Coroutines
fork[is Space][body is Block] ## returns the coroutine which can be awaited
await [coro is Coroutine] ## waits for the coroutine to finish and gets the return value

## Continuations
callcc[is Space][body is Block]
## interestingly enough each function callframe is implicitly wrapped in this to implement "return"
## and every loop is wrapped in TWO of these to implement "break" and "continue"

let [varname is Symbol] = [expression]
## defines variables in the current scope
```
