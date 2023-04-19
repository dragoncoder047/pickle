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
    constructor(type, content) {
        this.type = type;
        this.content = content;
    }
    toJSON() {
        return {
            type: this.type,
            content: this.content
        };
    }
}

class PickleTokenizer {
    constructor(string) {
        this.string = string;
        this.i = 0;
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
    errorchar() {
        this.i++;
        return new PickleToken("error", this.peek(-1));
    }
    nextToken() {
        if (this.done()) return undefined;
        // Try colon block string, to allow colon in operators
        if (this.testRE(/^:\s*\n/)) {
            var i = this.i;
            var lines = [];
            this.chompRE(/^:\s*\n/);
            var indent = this.chompRE(/^\s+/);
            if (!indent) {
                this.i = i;
                return new PickleToken("error", this.chompRE(/^:\s*\n/)[0]);
            }
            indent = indent[0];
            while (true) {
                var line = this.chompRE(/^[^\n]*/);
                if (line) lines.push(line[0]);
                if (!this.chomp(indent)) {
                    indent = this.chompRE(/^\s*/);
                    if (indent) return new PickleToken("error", indent[0]);
                    else break;
                }
            }
            return new PickleToken("string.block", lines.join("\n"));
        }
        const TOKEN_PAIRS = [
            { type: "comment.line", re: /^#[^\n]*?/, significant: false },
            { type: "comment.block", re: /^###[\s\S]*###/, significant: false },
            { type: "space", re: /^(?!\n)\s+/, significant: false },
            { type: "eol", re: /^[;\n]/, significant: true, groupNum: 0 },
            { type: "symbol", re: /^[a-z_][a-z0-9_]*\??/i, significant: true, groupNum: 0 },
            { type: "string.quote", re: /^(["'])((?:\\.|(?!\\|\1).)*)\1/, significant: true, groupNum: 2 },
            { type: "operator", re: /^[-~`!@$%^&*_+=[\]|\\:<>,.?/]*/, significant: true, groupNum: 0 },
            { type: "paren", re: /^[()]/, significant: true, groupNum: 0 },
        ]
        for (var { type, re, significant, groupNum } of TOKEN_PAIRS) {
            if (this.testRE(re)) {
                var match = this.chompRE(re);
                if (significant) return new PickleToken(type, match[groupNum]);
                else return this.nextToken();
            }
        }
        // Try special literal strings, quoted and block
        if (this.testRE(/^{/)) {
            var j = 0, depth = 0, string = "";
            do {
                var ch = this.peek(j);
                if (ch == undefined) return this.errorchar();
                if (ch == "{") depth++;
                else if (ch == "}") depth--;
                string += ch;
                j++;
            } while (depth > 0);
            this.i += j;
            return new PickleToken("string.curly", string.slice(1, -1));
        }
        return this.errorchar();
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