from dataclasses import dataclass
from typing import Callable, Any
from .object import Symbol, Pattern, Var, Space, Repeat, Alternate


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
        right=False) -> Callable[[Callable[[dict[Symbol, Any]], Any]], Pattern]:
    """Helper decorator to return a Pattern."""
    def pat_inner(handler: Callable[[dict[Symbol, Any]], Any]) -> Pattern:
        return Pattern(precedence, pattern, handler, right, macro)
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
    """Try to match the mattern at the specified index in the line,
    or return None if it doesn't match."""
    # pylint:disable=assignment-from-none
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
            case Repeat():
                # try to match min repeats first;
                # if there isn't enough for that even then bail
                for _ in range(entry.min):
                    result = match_one(line, source_pat, entry.what, line_i)
                    if result is None:
                        return None
                    bindings |= result.bindings
                    line_i = result.end
                if entry.max is not None:
                    more = entry.max - entry.min
                else:
                    more = -1
                if entry.greedy:
                    # NOTE HERE: it is NOT 'more > 0' because more == -1 if infinite repeats allowed
                    while more != 0 and line_i < len(line):
                        result = match_one(
                            line, source_pat, entry.what, line_i)
                        if result is None:
                            # hit max number of repeats
                            break
                        bindings |= result.bindings
                        line_i = result.end
                        more -= 1
                else:
                    # NOTE HERE: it is NOT 'more > 0' because more == -1 if infinite repeats allowed
                    while more != 0 and line_i < len(line):
                        # try to match remainder without more
                        # if it works, stop (not greedy)
                        if without := match_one(line, source_pat, pattern[patt_i + 1:], line_i):
                            bindings |= without.bindings
                            line_i = without.end
                            patt_i = len(pattern)
                            break
                        if result := match_one(line, source_pat, entry.what, line_i):
                            bindings |= result.bindings
                            line_i = result.end
                        else:
                            return None
                        more -= 1
            case Space():
                if isinstance(elem, Space):
                    # greedy
                    line_i += 1
            case Var():
                if entry.use_cls:
                    success = isinstance(elem, entry.cls)
                else:
                    success = elem == entry.cls
                if success:
                    line_i += 1
                    bindings[entry.var] = elem
                else:
                    return None
            case _:
                if entry != elem:
                    return None
                line_i += 1
        patt_i += 1
    if patt_i >= len(pattern):
        return Match(source_pat, start, line_i + 1, bindings)
    return None


def match(line: list, patterns: list[Pattern]) -> Match:
    """Finds the best match using the highest-precedence pattern that matches."""
    for pattern in sorted(patterns):
        matches: list[Match] = []
        for i in range(len(line)):
            if option := match_one(line, pattern, pattern.pattern, i):
                matches.append(option)
        if matches:
            return matches[-1 if pattern.right else 0]
    return None

if __name__ == "__main__":
    test_line = [0, 1, 2, 1, 2, 1, 2, 77]
    patts = [
        Pattern(1, [Repeat([1, 2], 1, None, False), Var("foo", int)], None),
    ]
    m = match(test_line, patts)
    test_line[m.start:m.end] = ["foo"]
    print(m)
    print(test_line)
