from dataclasses import dataclass, field
from itertools import islice
from typing import Callable, Any
from .object import Symbol, Pattern, Var, Space, Repeat, Alternate


@dataclass
class MatchResult:
    """Structure used to hold the result of a pattern-match call."""
    pattern: Pattern
    start: int
    end: int
    bindings: dict[Symbol, Any]

    def __len__(self):
        return self.end - self.start


class Label:
    "Used as a sentinel label for jump instructions."
    _id_counter = 0

    def __init__(self) -> None:
        self.id = None

    def __repr__(self) -> str:
        if self.id is None:
            self.id = Label._next_id()
        return f"<L{self.id}>"

    @classmethod
    def _next_id(cls):
        cls._id_counter += 1
        return cls._id_counter


class Inst:
    "Base class for compiled pattern Instruction lists."


@dataclass
class MatchOne(Inst):
    "Match one specific literal, or fail if it doesn't match."
    what: Var


@dataclass
class Goto(Inst):
    "Represents an instruction to go to a specific label or labels in the code."
    where: list[Label]


@dataclass
class SetupCounter(Inst):
    "Represents an instruction to set a counter."
    var: Label
    what: int


@dataclass
class DecrementCounter(Inst):
    "Represents an instruction to decrement a counter, and jump to a label if it is 0."
    var: Label
    exit: Label


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


def compile_pattern(pattern: Pattern) -> list[Inst]:
    "Compiles a pattern into a list of Instructions."
    out: list[Inst] = []
    try:
        iter(pattern)
    except TypeError:
        pattern = [pattern]
    for elem in pattern:
        top, end = Label(), Label()
        match elem:
            case Var():
                out.append(MatchOne(elem))
            case Space():
                out.extend([Goto([top, end]), top,
                           MatchOne(Var(None, Space)), end])
            case Repeat(what, minrep, maxrep, greedy):
                cinner = compile_pattern(what)
                out.extend(cinner * minrep)
                pair = (top, end) if greedy else (end, top)
                if maxrep is not None:
                    rep = maxrep - minrep
                    counter = Label()
                    out.extend([
                        SetupCounter(counter, rep),
                        Goto(pair),
                        top,
                        DecrementCounter(counter, end),
                        *cinner,
                        Goto([top]),
                        end])
                else:
                    out.extend([Goto(pair), top, *cinner, Goto([top]), end])
            case Alternate(options):
                rem: list[Inst] = []
                places: list[Label] = []
                for option in options:
                    here = Label()
                    rem.extend([here, *compile_pattern(option), Goto([end])])
                    places.append(here)
                out.extend([Goto(places), *rem, end])
            case _:
                out.append(MatchOne(Var(None, elem, False)))
    return out


@dataclass
class Thread:
    "Class used to hold match state data for a match program."
    bindings: dict[Symbol, Any]
    start: int
    pos: int = 0
    loop_counters: dict[Label, int] = field(default_factory=dict)


def _addthread(tlist: list[Thread], prog: list[Inst], oldthread: Thread, newpos: int):
    if any(t.pos == newpos for t in tlist):
        return
    if newpos >= len(prog):
        tlist.insert(0, Thread(oldthread.bindings.copy(), oldthread.start,
                               newpos, oldthread.loop_counters.copy()))
        return
    match prog[newpos]:
        case Goto(where):
            print(where)
            for lbl in where:
                _addthread(tlist, prog, oldthread, prog.index(lbl) + 1)
        case SetupCounter(var, what):
            oldthread.loop_counters[var] = what
            _addthread(tlist, prog, oldthread, newpos)
        case DecrementCounter(var, out):
            oldthread.loop_counters[var] -= 1
            if oldthread.loop_counters[var] < 0:
                _addthread(tlist, prog, oldthread, prog.index(out) + 1)
            else:
                _addthread(tlist, prog, oldthread, newpos)
        case Label():
            _addthread(tlist, prog, oldthread, newpos)
        case _:
            tlist.append(Thread(oldthread.bindings.copy(), oldthread.start,
                                newpos, oldthread.loop_counters.copy()))


def _match_one(line: list, pattern: Pattern, prog: list[Inst], start: int) -> MatchResult | None:
    threads: list[Thread] = []
    results: list[MatchResult] = []
    threads.append(Thread({}, start))
    for i, item in islice(enumerate(line), start, None):
        nlist: list[Thread] = []
        for thread in threads:
            if thread.pos >= len(prog):
                # no MATCH instruction, just fall off end => match
                results.append(MatchResult(
                    pattern, thread.start, i, thread.bindings))
                print("got match", results[-1])
                # cut off lower priority threads
                continue
            match prog[thread.pos]:
                case MatchOne(what):
                    if (isinstance(item, what.cls)
                        if what.use_cls
                            else what.cls == item):
                        if what.var is not None:
                            thread.bindings[what.var] = item
                        print("old list", nlist)
                        _addthread(nlist, prog, thread, thread.pos + 1)
                        print("new list", nlist)
                    else:
                        print("thread at pos", thread.pos,
                              "died", item, "!=", what.cls)
                case inst:
                    raise RuntimeError(inst)
        threads = nlist
    # Clean up threads that matched right on the end
    for thread in threads:
        if thread.pos >= len(prog):
            # no MATCH instruction, just fall off end => match
            results.append(MatchResult(
                pattern, thread.start, len(line) - 1, thread.bindings))
    if results:
        results = sorted(results, key=len)
        return results[0] if pattern.greedy else results[-1]
    return None


def match_pattern(line: list, pattern: Pattern) -> MatchResult | None:
    "Returns the best match for the pattern in the line."
    compiled = compile_pattern(pattern)
    for i in range(len(line)):
        if result := _match_one(line, pattern, compiled, i):
            return result
    return None


if __name__ == "__main__":
    import pprint
    test_line = [0, 1, 2, 1, 2, 1, 2, 1, 2, 1, 2, 1, 2,
                 1, 2, 1, 2, 1, 2, 1, 2, 1, 2, 1, 2, 1, 2, 77]
    p0 = Pattern(1, [Repeat([1, 2], 1, None), Var("foo", int)], None)
    pprint.pprint(compile_pattern(p0))
    Label._id_counter = 0  # pylint:disable=protected-access
    # """
    m = match_pattern(test_line, p0)
    pprint.pprint(m)
    # """
    test_line[m.start:m.end] = ["replaced"]
    print(test_line)
    # """
