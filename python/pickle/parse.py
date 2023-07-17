import re
import enum
from typing import NoReturn, Any
from .errors import ParseFail

UNESCAPE_MAP = {
    "b": "\b",
    "t": "\t",
    "n": "\n",
    "v": "\v",
    "f": "\f",
    "r": "\r",
    "a": "\a",
    "o": "{",
    "c": "}",
    "e": "\x1b",
    "\n": ""
}

ESCAPE_MAP = {
    "\\": "\\",
    "\b": "b",
    "\t": "t",
    "\n": "n",
    "\v": "v",
    "\f": "f",
    "\r": "r",
    "\a": "a",
    "{": "o",
    "}": "c",
    "\x1b": "e"
}


def unescape(ch: str) -> str:
    return UNESCAPE_MAP.get(ch, ch)


def escape(ch: str) -> str:
    if ch in ESCAPE_MAP:
        return "\\" + ESCAPE_MAP[ch]
    return ch


class SourceLocation:
    """An object that holds the position in the file that generated the token."""

    def __init__(self, filename: str, line: int, column: int):
        self.line = line
        self.column = column
        self.filename = filename

    def as_tb_line(self):
        # Python style
        # return f"file {self.filename}, near line {self.line}"
        # C style
        return f"{self.filename}:{self.line}:{self.column}"

    def __iadd__(self, other: "SourceLocation"):
        if not isinstance(other, SourceLocation):
            return NotImplemented
        self.line += other.line
        self.column += other.column
        return self


class TokenKind(enum.Enum):
    ERR = 0
    STR = 1
    PAR = 2
    EOL = 3
    SYM = 4
    SPC = 5


class Token:
    """A parse token generated when parsing PICKLE code."""

    def __init__(
            self,
            kind: TokenKind,
            content: str,
            start: SourceLocation,
            end: SourceLocation,
            message: str = None):
        self.kind = kind
        self.content = content
        self.message = message
        self.start = start
        self.end = end

    def __repr__(self) -> str:
        msgpart = " (" + self.message + ")" if self.message else ""
        return f"<{self.kind.name} at {self.start.as_tb_line()}\t{self.content!r}{msgpart}>"


class Tokenizer:
    """The thing that does all the work when tokenizing Pickle code."""

    def __init__(self, string="", filename="", offset=SourceLocation(None, 0, 0)):
        self.offset = offset
        self.bi = 0
        self.i = 0
        self.last_token: Token = None
        self.string = string
        self.filename = filename
        self.position_stack: list[int] = []

    def at(self, i=0):
        """Returns the character the tokenizer is sitting on, with an optional offset."""
        return self.string[self.i+i]

    def string_at(self, i=0):
        """Returns the string remaining to parse, with an optional offset."""
        return self.string[self.i+i:]

    def __enter__(self):
        self.position_stack.append(self.i)

    def __exit__(self, type_, value, traceback):
        i = self.position_stack.pop()
        if isinstance(value, ParseFail):
            self.i = i
            return True

    def __bool__(self):
        return self.i < len(self.string)

    def chomp(self, what: str | re.Pattern) -> str | re.Match | None:
        """Chomps the prefix off, advances the stream, and returns what was chomped.
        If nothing was chomped, return None."""
        if isinstance(what, str):
            if self.string_at().startswith(what):
                self.i += len(what)
                return what
        elif isinstance(what, re.Pattern):
            if match := what.match(self.string, self.i):
                self.i += match.end() - match.start()
                return match
        return None

    def chomp_re(self, what: str) -> re.Match | None:
        return self.chomp(re.compile(what))

    def chomp_or_fail(self, what: str | re.Pattern) -> str | re.Match | NoReturn:
        """Try chomp(), and if it returns None, raise ParseFail."""
        result = self.chomp(what)
        if result is None:
            raise ParseFail
        return result

    def chomp_re_or_fail(self, what: str) -> re.Match | NoReturn:
        return self.chomp_or_fail(re.compile(what))

    def try_funs_or_fail(self, *functions) -> Any | NoReturn:
        for fun in functions:
            with self:
                if (result := fun()) is not None:
                    return result
        raise ParseFail

    def consume_greedy(self, function):
        while True:
            try:
                function()
            except ParseFail:
                return

    def error(self, offending="", message="") -> Token:
        """Creates an error token when parsing fails."""
        if self.i == self.bi:
            self.i += 1
        return self.make_token(
            TokenKind.ERR,
            offending or self.string[self.bi:self.i],
            message or f"unexpected {self.at(-1)}")

    def make_token(self, kind: TokenKind, content: str, message=""):
        """Creates a token and initializes the line and column numbers of the start and end."""
        here = SourceLocation(self.filename, 1, 1)
        start: SourceLocation = None
        for i, ch in enumerate(self.string[:self.i]):
            if i == self.bi:
                start = SourceLocation(self.filename, here.line, here.column)
            if ch == "\n":
                here.line += 1
                here.column = 0
            else:
                here.column += 1
        here += self.offset
        start += self.offset
        return Token(kind, content, start, here, message)

    def __try_whitespace(self) -> Token | NoReturn:
        match = (
            # block comment
            self.chomp_re(
                r"(?<!#)(###)(\S*?)(#+)(?!#)[\s\S\n\r]*?(?<!#)\3\2\1(?!#)")
            # line comment
            or self.chomp_re(r"##[^\n]*")
            # general whitespace
            or self.chomp_re_or_fail(r"((?!\n)\s)+"))
        return self.make_token(TokenKind.SPC, match.group())

    def __try_colon_block(self) -> Token | NoReturn:
        self.chomp_or_fail(":")
        self.consume_greedy(self.__try_whitespace)
        self.chomp_or_fail("\n")
        indent = self.chomp_re(r"((?!\n)\s)+")
        if indent is None:
            return self.error(message="expected indent after colon and EOL")
        indent = indent.group()
        if re.fullmatch(r"(\s)\1*", indent) is None:
            return self.error(indent, "mix of tabs and spaces indenting block")
        lines = []
        while True:
            if line := self.chomp_re(r"[^\r\n]*"):
                lines.append(line.group(0))
            else:
                lines.append("")
            if self.chomp_re(r"\r|\n|\r\n") is None:
                break
            if not self.chomp(indent):
                if bad_indent := self.chomp_re(r"(((?!\n)\s)*)(?=\S)"):
                    bad_indent = bad_indent.group()
                    if len(bad_indent) > 0:
                        return self.error(bad_indent, "unexpected unindent")
                    else:
                        break
        return self.make_token(TokenKind.STR, "\n".join(lines))

    def __try_symbol_paren_eol(self) -> Token | NoReturn:
        if match := self.chomp_re(r"""(?![\(\{\[\]\}\);"'])\S"""):
            return self.make_token(TokenKind.SYM, match.group())
        if match := self.chomp_re(r"[\(\[\]\)]"):
            return self.make_token(TokenKind.PAR, match.group())
        match = self.chomp_re_or_fail(r"[;\s]+")
        return self.make_token(TokenKind.EOL, None)

    def __try_brace_string(self) -> Token | NoReturn:
        self.chomp_or_fail("{")
        depth = 1
        out = ""
        while self:
            char = self.at()
            self.i += 1
            if char == "{":
                depth += 1
            if char == "}":
                depth -= 1
            if depth == 0:
                return self.make_token(TokenKind.STR, out)
            out += char
        return self.error(out, "unexpected EOF inside {")

    def __try_quote_string(self) -> Token | NoReturn:
        quote = self.chomp_re_or_fail(r"""["']""").group()
        out = ""
        while self:
            char = self.at()
            self.i += 1
            if char in "\r\n":
                return self.error(message="unexpected newline in string"
                                  " (use \\ to escape newlines)")
            if char == "\\":
                char = unescape(self.at())
                self.i += 1
            elif char == quote:
                return self.make_token(TokenKind.STR, out)
            out += char
        return self.error(out, "unexpected EOF in string")

    def next_token(self) -> Token | None:
        """Return the next token in the stream, or None if the stream is exhausted."""
        if not self:
            return None
        self.bi = self.i
        try:
            return self.try_funs_or_fail(
                self.__try_whitespace,
                self.__try_colon_block,
                self.__try_symbol_paren_eol,
                self.__try_brace_string,
                self.__try_quote_string)
        except ParseFail:
            return self.error()


class Parser:
    """The second pass in PICKLE parsing, uses the token stream to """


if __name__ == "__main__":
    x = Tokenizer(
        r"""
"Hello I am a string";;;;;;;;;;;;;
{Hello I am a {nested
{extremely
{deeply}}} string
with newlines}
### I am a line comment ###
' this should be a string not a comment '
###11### This is
"a block comment"
###22### nested comment ###22###
end of comment ###11###
pattern [x is Number]+[y is Number]j:
    Complex.new $x $y
"I am a string with \n embedded \e \a \o\o\o\c\c\c escapes"
"I\
have\
embedded\
newlines\
escaped"
"i am an unclosed string
bwni:
pass
}}}}}}}}}}} ## errors
""",
        filename="test")
    while x:
        print(x.next_token())
