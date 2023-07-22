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

    def __len__(self):
        return self.end - self.start

    def __or__(self, other: "Match"):
        return (Match(self.pattern, self.start, other.end, self.bindings | other.bindings)
                if isinstance(other, Match) else NotImplemented)


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


def _match_alternate(line: list, source_pat: Pattern, options: list, start: int) -> Match | None:
    for option in options:
        if result := match_one(line, source_pat, option, start):
            return result
    # No options matched, so the entire pattern doesn't match
    return None


def _match_space(line: list, start: int):
    return isinstance(line[start], Space)


def _match_var(line: list, source_pat: Pattern, what: Var, start: int):
    elem = line[start]
    if what.use_cls:
        success = isinstance(elem, what.cls)
    else:
        success = elem == what.cls
    if success:
        return Match(source_pat, start, start+1, {what.var: elem})
    return None


def _match_repeat_fixed(line: list, source_pat: Pattern, what: list, start: int, num_repeats: int):
    result = None
    i = start
    for _ in range(num_repeats):
        result = match_one(line, source_pat, what, i)
        if result is None:
            break
        i = result.end
    return result


def _match_repeat_greedy(line: list, source_pat: Pattern, what: list, start: int, max_rep: int):
    # NOTE HERE: it is NOT 'max_rep > 0' because max_rep == -1 if infinite repeats allowed
    i = start
    result = None
    while max_rep != 0 and i < len(line):
        result = match_one(line, source_pat, what, i)
        if result is None:
            # hit max number of repeats
            break
        max_rep -= 1
        assert result.start == i
        i = result.end
    return result


def _match_repeat_lazy(
        line: list,
        source_pat: Pattern,
        what: list,
        rest_of_pat: list,
        start: int,
        max_rep: int):
    # NOTE HERE: it is NOT 'max_rep > 0' because max_rep == -1 if infinite repeats allowed
    i = start
    result = None
    while max_rep != 0 and i < len(line):
        # try to match remainder without more
        # if it works, stop (not greedy)
        if without := match_one(line, source_pat, rest_of_pat, i):
            assert result.start == i
            i = without.end
            result |= without
            break
        if result := match_one(line, source_pat, what, i):
            assert result.start == i
            i = result.end
        else:
            return None
        max_rep -= 1
    return result


def match_one(line: list, source_pat: Pattern, pattern: list, start: int) -> Match | None:
    """Try to match the mattern at the specified index in the line,
    or return None if it doesn't match."""
    patt_i = 0
    line_i = start
    bindings = {}
    for patt_i, item in enumerate(pattern):
        elem = line[line_i]
        result = None
        match item:
            case Alternate(options=opts):
                result = _match_alternate(line, source_pat, opts, line_i)
            case Repeat(min=min_rep, max=max_rep, what=what, greedy=greedy):
                # try to match min repeats first;
                # if there isn't enough for that, then bail
                result = _match_repeat_fixed(
                    line, source_pat, what, line_i, min_rep)
                if result is None:
                    return None
                more = max_rep - min_rep if max_rep else -1
                if greedy:
                    more = _match_repeat_greedy(
                        line, source_pat, what, line_i, more)
                else:
                    more = _match_repeat_lazy(
                        line, source_pat, what, pattern[patt_i+1:], line_i, more)
                if more is not None:
                    result |= more
            case Space():
                result = _match_space(line, line_i)
            case Var() as entry:
                result = _match_var(line, source_pat, entry, line_i)
            case _ as entry:
                result = entry != elem
        if not result:
            return None
        if isinstance(result, Match):
            bindings |= result.bindings
            assert result.start == line_i
            line_i = result.end
        else:
            line_i += 1
        if line_i >= len(line):
            break
    if patt_i >= len(pattern):
        return Match(source_pat, start, line_i + 1, bindings)
    return None


def match_patterns(line: list, patterns: list[Pattern]) -> Match | None:
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
    # """
    patts = [
        Pattern(1, [Repeat([1, 2], 1, None), Var("foo", int)], None),
    ]
    m = match_patterns(test_line, patts)
    test_line[m.start:m.end] = ["replaced"]
    print(m)
    print(test_line)
    # """
