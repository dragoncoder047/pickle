from typing import Literal
from dataclasses import dataclass


@dataclass
class POperator:
    "Data to define an operator that an object responds to."
    symbol: str
    arity: Literal[-1, 0, 1]
    precedence: int


class PObj:
    "Base class for PICKLE objects."

    def __init__(self):
        self.prototypes: list[PObj] = []
        self.properties: dict[PObj, PObj] = {}
        self.operators: dict[POperator, PObj] = {}

    def __eq__(self, other: "PObj") -> bool:
        if self is other:
            return True
        if type(self) is not type(other):
            return False
        return self.properties == other.properties and self.prototypes == other.prototypes


class PKVPair(PObj):
    def __init__(self, key=None, val=None):
        super().__init__()
        self.key = key
        self.val = val

class PRecord(PObj):
    "Base class for sequence objects"

    def __init__(self, items: list[PObj]):
        super().__init__()
        self.list: list[PObj] = list(items)

    def __eq__(self, other: PObj) -> bool:
        return super().__eq__(other) and self.list == other.list

    def __len__(self):
        return len(self.list)

    def append(self, *what):
        for x in what:
            self.list.append(x)

    def __iadd__(self, item):
        self.append(item)

    def __iter__(self):
        return iter(self.list)

    def insert(self, i: int, what: PObj):
        self.list.insert(i, what)

    def pop(self, i=-1) -> PObj:
        return self.list.pop(i)
    
    def find_entry(self, o: PObj) -> PObj | None:
        for x in self.list:
            if isinstance(x, PKVPair):
                if x.key == o:
                    return x.val
        return None
    
    def __getitem__(self, i):
        return self.list[i]
