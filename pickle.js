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
    constructor(type, content, start, end) {
        var types = type.split(".");
        this.type = types[0];
        this.subtypes = types.slice(1);
        this.content = content;
        this.start = start;
        this.end = end;
    }
    toJSON() {
        return {
            type: this.type,
            subtypes: this.subtypes,
            content: this.content,
            start: this.start,
            end: this.end,
        };
    }
}

class PickleTokenizer {
    constructor(string) {
        this.string = string;
        this.i = 0;
        this.beginning = null;
    }
    lineColumn() {
        var before = this.string.slice(0, this.i);
        var doneLines = before.split("\n");
        var line = doneLines.length;
        var col = doneLines.at(-1).length;
        return { line, col };
    }
    test(string) {
        return this.string.slice(this.i).startsWith(string);
    }
    chomp(string) {
        if (!this.test(string)) return undefined;
        this.i += string.length;
        return string;
    }
    testRE(re) {
        return re.test(this.string.slice(this.i));
    }
    chompRE(re) {
        if (!this.testRE(re)) return undefined;
        var match = re.exec(this.string.slice(this.i));
        this.i += match[0].length;
        return match;
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
        return this.makeToken("error", this.peek(-1));
    }
    makeToken(type, content) {
        return new PickleToken(type, content, this.beginning, this.lineColumn());
    }
    nextToken() {
        if (this.done()) return undefined;
        this.beginning = this.lineColumn();
        // Try colon block string, to allow colon in operators
        if (this.testRE(/^:\s*\n/)) {
            var i = this.i;
            var lines = [];
            this.chompRE(/^:\s*\n/);
            var indent = this.chompRE(/^\s+/);
            if (!indent) {
                this.i = i;
                return this.makeToken("error", this.chompRE(/^:\s*\n/)[0]);
            }
            indent = indent[0];
            while (true) {
                var line = this.chompRE(/^[^\n]*/);
                lines.push(line[0] || "");
                if (!this.chomp("\n")) return this.errorToken();
                if (!this.chomp(indent)) {
                    indent = this.chompRE(/^\s*/);
                    if (indent && indent[0].length > 0) return this.makeToken("error", indent[0]);
                    else break;
                }
            }
            return this.makeToken("string.block", lines.join("\n"));
        }
        const TOKEN_PAIRS = [
            { type: "comment.line", re: /^#[^\n]*?/, significant: false },
            { type: "comment.block", re: /^###[\s\S]*###/, significant: false },
            { type: "space", re: /^(?!\n)\s+/, significant: false },
            { type: "eol", re: /^[;\n]/, significant: true, groupNum: 0 },
            { type: "singleton", re: /^(true|false|nil)/, significant: true, groupNum: 0 },
            { type: "number.complex", re: /^[+-]?\d+[+-]\d+j/, significant: true, groupNum: 0 },
            { type: "number.rational", re: /^[+-]?\d+\/[+-]\d+/, significant: true, groupNum: 0 },
            { type: "number.float", re: /^[+-]?[0-9]+e[+-]\d+/i, significant: true, groupNum: 0 },
            { type: "number.integer", re: /^[+-]?([0-9]+|0x[0-9a-f]+|0b[01]+)/i, significant: true, groupNum: 0 },
            { type: "symbol", re: /^[a-z_][a-z0-9_]*\??/i, significant: true, groupNum: 0 },
            { type: "string.quote", re: /^(["'])((?:\\.|(?!\\|\1).)*)\1/, significant: true, groupNum: 2 },
            { type: "operator", re: /^[-~`!@$%^&*_+=[\]|\\:<>,.?/]*/, significant: true, groupNum: 0 },
            { type: "paren", re: /^[\(\)]/, significant: true, groupNum: 0 },
        ]
        for (var { type, re, significant, groupNum } of TOKEN_PAIRS) {
            if (this.testRE(re)) {
                var match = this.chompRE(re);
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
    var out = [];
    while (!x.tokenizer.empty()) {
        out.push(x.tokenizer.nextToken());
    }
    return out;
}