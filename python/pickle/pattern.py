from dataclasses import dataclass
from typing import Callable, Any
from .object import Symbol, Pattern, Var, Space, Optional, Alternate


@dataclass
class Match:
    """Structure used to hold the result of a pattern-match call."""
    pattern: Pattern
    start: int
    end: int
    bindings: dict[Symbol, Any]


def pat(
        *pattern,
        precedence=1,
        macro=False,
        right=False,
        greedy=True) -> Callable[[Callable[[dict[Symbol, Any]], Any]], Pattern]:
    """Helper decorator to return a Pattern."""
    def pat_inner(handler: Callable[[dict[Symbol, Any]], Any]) -> Pattern:
        return Pattern(precedence, pattern, handler, right, macro, greedy)
    return pat_inner


def partition_and_sort(patterns: list[Pattern]) -> tuple[list[Pattern], list[Pattern]]:
    """Partitions the list of patterns into a tuple (macros, non_macros), and sorts both."""
    macros, non_macros = [], []
    for patt in patterns:
        if patt.macro:
            macros.append(patt)
        else:
            non_macros.append(patt)
    return sorted(macros), sorted(non_macros)


def match_one(line: list, source_pat: Pattern, pattern: list, start: int) -> Match | None:
    """Try to match the mattern at the specified index in the line, or return None if it doesn't match."""
    patt_i = 0
    line_i = start
    bindings = {}
    while patt_i < len(pattern) and line_i < len(line):
        elem = line[line_i]
        match entry := pattern[patt_i]:
            case Alternate():
                for option in entry.options:
                    if result := match_one(line, source_pat, option, line_i):
                        bindings |= result.bindings
                        line_i = result.end
                        break
                else:
                    # No options matched, so the entire pattern doesn't match
                    return None
            case Optional():
                if entry.greedy:
                    if result := match_one(line, source_pat, entry.what, line_i):
                        bindings |= result.bindings
                        line_i = result.end
                    # else pass, it is optional
                else:
                    # try to match without first
                    if without := match_one(line, source_pat, pattern[patt_i + 1:], line_i):
                        bindings |= without.bindings
                        line_i = without.end
                    elif result := match_one(line, source_pat, entry.what, line_i):
                        bindings |= result.bindings
                        line_i = result.end
                    else:
                        return None
            case Space():
                if isinstance(elem, Space) or elem is Space:
                    ..., ..., ..., ..., ..., ..., ..., ..., ..., ..., ..., ..., ..., ..., ..., 


def match(line: list, patterns: list[Pattern]) -> Match:
    """Finds the best match using the highest-precedence pattern that matches."""
    for pattern in sorted(patterns):
        matches: list[Match] = []
        for i in range(len(pattern)):
            if option := match_one(line, pattern, pattern.pattern, i):
                matches.append(option)
        if matches:
            return matches[-1 if pattern.right else 0]
    return None
