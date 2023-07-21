import functools
from typing import Callable, Any
from dataclasses import dataclass


@dataclass
class Symbol:
    """String-like thing that represents a keyword or atom."""
    name: str


@dataclass
@functools.total_ordering
class Pattern:
    """The core object that manages pattern-matching."""
    precedence: int
    pattern: list
    handler: Callable[[dict[Symbol, Any]], Any]
    right: bool = False
    macro: bool = False

    def __lt__(self, other):
        if isinstance(other, Pattern):
            return other.precedence < self.precedence
        return NotImplemented

    def __eq__(self, other):
        if isinstance(other, Pattern):
            return other.precedence == self.precedence
        return NotImplemented

    def __len__(self):
        return len(self.pattern)


@dataclass
class Var:
    """Something that isn't a literal in a pattern, that binds to a variable"""
    var: Symbol
    cls: type
    use_cls: bool = True


@dataclass
class Space:
    """Dummy value used to represent whitespace in a a pattern."""
    comment: str = ""


@dataclass
class Repeat:
    """Value used in patterns to indicate the element can be repeated."""
    what: list[Any]
    min: int
    max: int | None
    greedy: bool = True

    def __post_init__(self):
        if self.max is not None:
            assert self.max >= self.min


@dataclass
class Alternate:
    """Value used in patterns to indicate multiple options."""
    options: list[Any]
