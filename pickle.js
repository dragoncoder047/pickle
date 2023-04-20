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
        case '\n': return '';
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
    test(what) {
        if (typeof what === "string") return this.string.slice(this.i).startsWith(what);
        else if (what instanceof RegExp) return what.test(this.string.slice(this.i));
        else return false;
    }
    chomp(what) {
        if (!this.test(what)) return undefined;
        if (typeof what === "string") {
            this.i += what.length;
            return what;
        }
        else if (what instanceof RegExp) {
            var match = what.exec(this.string.slice(this.i));
            this.i += match[0].length;
            return match;
        }
        else return undefined;
    }
    done() {
        return this.i >= this.string.length;
    }
    peek(i = 0) {
        var j = this.i + i;
        if (j >= this.string.length) return undefined;
        return this.string[j];
    }
    errorToken(message = "") {
        console.debug("errorToken()", message);
        // always advance to allow more tokenizing
        if (this.bi == this.i) this.i++;
        return this.makeToken("error", this.string.slice(this.bi, this.i), message || `unexpected ${this.peek(-1)}`);
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
            console.debug("trying colon string");
            var i = this.i;
            var lines = [];
            this.chomp(/^:\s*\n/);
            var indent = this.chomp(/^\s+/);
            if (!indent) {
                this.i = i;
                return this.makeToken("error", this.chomp(/^:\s*\n/)[0], "expected indent after colon");
            }
            indent = indent[0];
            var ensure_same = /^([\t ])\1*/.exec(indent);
            if (!ensure_same) return this.makeToken("error", indent, "mix of tabs and spaces indenting block");
            console.debug("indent is", indent.length, indent[0] == "\t" ? "tabs" : "spaces");
            while (true) {
                var line = this.chomp(/^[^\n]*/);
                lines.push(line[0] || "");
                console.debug("got line", line[0]);
                if (!this.chomp("\n")) break;
                if (!this.chomp(indent)) {
                    var b = this.lineColumn();
                    var bi = this.i;
                    var badIndent = this.chomp(/^(((?!\n)\s)*)(?=\S)/);
                    if (badIndent) {
                        if (badIndent[1].length > 0) {
                            this.beginning = b;
                            this.bi = bi;
                            return this.makeToken("error", badIndent[1], "unexpected unindent");
                        }
                        else break;
                    }
                }
            }
            return this.makeToken("string.block", lines.join("\n"));
        }
        const TOKEN_REGEXES = [
            { type: "comment.line", re: /^#[^\n]*/, significant: false },
            { type: "comment.block", re: /^###[\s\S]*###/, significant: false },
            { type: "paren", re: /^[\(\)\[\]]/, significant: true, groupNum: 0 },
            { type: "space", re: /^(?!\n)\s+/, significant: false },
            { type: "eol", re: /^[;\n]/, significant: true, groupNum: 0 },
            { type: "singleton", re: /^(true|false|nil)/, significant: true, groupNum: 0 },
            { type: "number.complex", re: /^-?[0-9]+(\.[0-9]+)?e[+-]\d+[+-][0-9]+(\.[0-9]+)?e[+-]\d+j/, significant: true, groupNum: 0 },
            { type: "number.rational", re: /^-?[0-9]+\/[0-9]+/, significant: true, groupNum: 0 },
            { type: "number.integer", re: /^-?([1-9][0-9]*|0x[0-9a-f]+|0b[01]+)/i, significant: true, groupNum: 0 },
            { type: "number.float", re: /^-?[0-9]+(\.[0-9]+)?(e[+-]\d+)?/i, significant: true, groupNum: 0 },
            { type: "symbol", re: /^[a-z_][a-z0-9_]*\??/i, significant: true, groupNum: 0 },
            { type: "symbol.operator", re: /^[-~`!@$%^&*_+=[\]|\\:<>,.?/]+/, significant: true, groupNum: 0 },
        ]
        for (var { type, re, significant, groupNum } of TOKEN_REGEXES) {
            console.debug("trying", type);
            if (this.test(re)) {
                var match = this.chomp(re);
                if (significant) return this.makeToken(type, match[groupNum]);
                else return this.nextToken();
            }
        }
        // Try strings
        if (this.test("{")) {
            console.debug("trying brace string");
            var j = 0, depth = 0, string = "";
            do {
                var ch = this.peek(j);
                console.debug("peek", ch, "depth=", depth);
                if (ch == undefined) return this.errorToken("unclosed {");
                if (ch == "{") depth++;
                else if (ch == "}") depth--;
                string += ch;
                j++;
            } while (depth > 0);
            this.i += j;
            return this.makeToken("string.curly", string.slice(1, -1));
        }
        else if (this.test(/^['"]/)) {
            console.debug("trying quote string");
            var q = this.chomp(/^['"]/)[0];
            var j = 0, string = "";
            while (true) {
                var ch = this.peek(j);
                console.debug("peek", ch);
                // newlines must be backslash escaped
                if (ch == undefined || ch == "\n") {
                    this.i += j;
                    return this.errorToken("unterminated string");
                }
                else if (ch == "\\") {
                    ch = unescape(this.peek(j + 1));
                    j++;
                }
                else if (ch == q) break;
                string += ch;
                j++;
            }
            this.i += j + 1;
            return this.makeToken("string.quote", string);
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