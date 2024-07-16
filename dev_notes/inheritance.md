# Basic ABC's

* Singleton
  * Nothing
  * Undefined
  * Done (EOF signal)
* Callable
  * Function/Closure
  * Continuation
  * BoundMethod
* Iterable (`for ... in ...` loops)
  * List
    * String
    * Bytevector
  * Set
  * Record
  * Range
* Iterator (`for ... of ...` loops)
* Numeric
  * Scalar
    * Float
    * Rational
    * Integer
  * Complex < Vector
* Symbol
  * Keyword
* CodeType
  * Expression < List
  * Block < List, String
  * Comment < String
  * Whitespace
  * EOL
  * Pattern < List
  * PatternCombinator
    * Repeat
    * Alternate
    * Match
    * Lookahead/Lookbehind
    * etc. 
* Error < Continuation
  * MathError
    * DivideByZeroError
  * LookupError
    * IndexError
    * KeyError
      * AttributeError
    * NameError
  * AssertionError
  * IOError
    * EOFError
    * FileNotFoundError
    * FIleExistsError
    * ImportError
      * NoModuleError < FileNotFoundError
      * NoExportError < LookupError
    * OSError
  * UserInterrupt
  * RuntimeError
    * RecursionError
    * SyntaxError
      * IndentationError
    * SystemError
  * OutOfMemoryError
  * TypeError
* Stream < Iterator
  * BytesStream
  * TextStream
  * ...everything else
