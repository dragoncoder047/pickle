class PickleError extends Error {
    constructor(message) {
        super(message);
        this.name = this.constructor.name;
    }
}

class PickleParseError extends PickleError { }

class PickleObject {
    constructor() {
        this.properties = new Map();
        this.operators = new Map();
        this.prototypes = new Map();
    }
}

const EOL_CHARS = ";\n\r";

function unescape(c) {
    switch (c) {
        case 'b': return '\b';
        case 't': return '\t';
        case 'n': return '\n';
        case 'v': return '\v';
        case 'f': return '\f';
        case 'r': return '\r';
        case 'a': return '\a';
        case 'o': return '{';
        case 'c': return '}';
        default: return c;
    }
}

function escape(c) {
    switch (c) {
        case '\b': return 'b';
        case '\t': return 't';
        case '\n': return 'n';
        case '\v': return 'v';
        case '\f': return 'f';
        case '\r': return 'r';
        case '\a': return 'a';
        case '{': return 'o';
        case '}': return 'c';
        default: return c;
    }
}

function needsEscape(c) {
    return "{}\b\t\n\v\f\r\a\\\"".indexOf(c) != -1;
}

class PickleToken {
    constructor(type, content, start, end, message = "") {
        var types = type.split(".");
        this.type = types[0];
        this.subtypes = types.slice(1);
        this.content = content;
        this.start = start;
        this.end = end;
        this.message = message;
    }
    toJSON() {
        return {
            type: this.type,
            subtypes: this.subtypes,
            content: this.content,
            start: this.start,
            end: this.end,
            message: this.message
        };
    }
}

class PickleTokenizer {
    constructor(string) {
        this.string = string;
        this.i = 0;
        this.beginning = null;
        this.bi = 0;
    }
    lineColumn() {
        var before = this.string.slice(0, this.i);
        var doneLines = before.split("\n");
        var line = doneLines.length;
        var col = doneLines.at(-1).length + 1;
        return { line, col };
    }
    test(string) {
        if (typeof string === "string") return this.string.slice(this.i).startsWith(string);
        else if (string instanceof RegExp) return string.test(this.string.slice(this.i));
        else return false;
    }
    chomp(string) {
        if (!this.test(string)) return undefined;
        if (typeof string === "string") {
            this.i += string.length;
            return string;
        }
        else if (string instanceof RegExp) {
            var match = string.exec(this.string.slice(this.i));
            this.i += match[0].length;
            return match;
        }
        else return undefined;
    }
    done() {
        return this.i >= this.string.length;
    }
    peek(i = 0) {
        return this.string[this.i + i];
    }
    errorToken() {
        // always advance to allow more tokenizing
        this.i++;
        return this.makeToken("error", this.string.slice(this.bi, this.i), `unexpected ${this.peek(-1)}`);
    }
    makeToken(type, content, message = "") {
        return new PickleToken(type, content, this.beginning, this.lineColumn(), message);
    }
    nextToken() {
        if (this.done()) return undefined;
        this.beginning = this.lineColumn();
        this.bi = this.i;
        // Try colon block string, to allow colon in operators
        if (this.test(/^:\s*\n/)) {
            var i = this.i;
            var lines = [];
            this.chomp(/^:\s*\n/);
            var indent = this.chomp(/^\s+/);
            if (!indent) {
                this.i = i;
                return this.makeToken("error", this.chomp(/^:\s*\n/)[0], "expected indent after colon");
            }
            indent = indent[0];
            while (true) {
                var line = this.chomp(/^[^\n]*/);
                lines.push(line[0] || "");
                if (!this.chomp("\n")) break;
                if (!this.chomp(indent)) {
                    var badIndent = this.chomp(/^((?!\n)\s)*\S/);
                    if (badIndent) {
                        if (badIndent[1].length > 0) return this.makeToken("error", badIndent[1]);
                        else break;
                    }
                }
            }
            return this.makeToken("string.block", lines.join("\n"));
        }
        const TOKEN_REGEXES = [
            { type: "comment.line", re: /^#[^\n]*/, significant: false },
            { type: "comment.block", re: /^###.*###/s, significant: false },
            { type: "paren", re: /^[\(\)\[\]]/, significant: true, groupNum: 0 },
            { type: "space", re: /^(?!\n)\s+/, significant: false },
            { type: "eol", re: /^[;\n]/, significant: true, groupNum: 0 },
            { type: "singleton", re: /^(true|false|nil)/, significant: true, groupNum: 0 },
            { type: "number.complex", re: /^-?[0-9]+(\.[0-9]+)?e[+-]\d+[+-][0-9]+(\.[0-9]+)?e[+-]\d+j/, significant: true, groupNum: 0 },
            { type: "number.rational", re: /^-?[0-9]+\/[0-9]+/, significant: true, groupNum: 0 },
            { type: "number.float", re: /^-?[0-9]+(\.[0-9]+)?(e[+-]\d+)?/i, significant: true, groupNum: 0 },
            { type: "number.integer", re: /^-?([1-9][0-9]*|0x[0-9a-f]+|0b[01]+)/i, significant: true, groupNum: 0 },
            { type: "symbol", re: /^[a-z_][a-z0-9_]*\??/i, significant: true, groupNum: 0 },
            { type: "string.quote", re: /^(["'])((\\.|(?!\1)[^\\])*)\1/, significant: true, groupNum: 2 },
            { type: "operator", re: /^[-~`!@$%^&*_+=[\]|\\:<>,.?/]*/, significant: true, groupNum: 0 },
        ]
        for (var { type, re, significant, groupNum } of TOKEN_REGEXES) {
            if (this.test(re)) {
                var match = this.chomp(re);
                if (significant) return this.makeToken(type, match[groupNum]);
                else return this.nextToken();
            }
        }
        // Try special literal strings, quoted and block
        if (this.test("{")) {
            var j = 0, depth = 0, string = "";
            do {
                var ch = this.peek(j);
                if (ch == undefined) return this.errorToken();
                if (ch == "{") depth++;
                else if (ch == "}") depth--;
                string += ch;
                j++;
            } while (depth > 0);
            this.i += j;
            return this.makeToken("string.curly", string.slice(1, -1));
        }
        return this.errorToken();
    }
}

class PickleParser {
    constructor(code) {
        this.tokenizer = new PickleTokenizer(code);
    }
}

function pickleParse(string) {
    var x = new PickleParser(string);
    throw 'todo';
}