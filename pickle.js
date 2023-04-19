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
        return { type: this.type, content: this.content };
    }
}

class PickleTokenizer {
    constructor(string) {
        this.string = string;
        this.i = 0;
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
    peek(i=0) {
        return this.string[this.i + i];
    }
    nextToken() {
        const TOKEN_PAIRS = [
            { type: "comment.line", re: /^#[^\n]*?/ },
            { type: "comment.block", re: /^###[\s\S]*###/ },
            { type: "space", re: /^(?!\n)\s+/ },
            { type: "symbol", re: /^(?<t>[a-z_][a-z0-9_]*\??)/i },
            { type: "string.quote", re: /^(["'])(?<t>(?:\\.|(?!\\|\1).)*)\1/ },
            { type: "operator", re: /^(?<t>([~`!@$%^&*-+=[\]|\\;,.<>?/]|:(?![ \t]+\n))*)/ },
            { type: "paren", re: /^(?<t>[()])/ },
        ]
        for (var { type, re } of TOKEN_PAIRS) {
            if (this.testRE(re)) {
                var match = this.chompRE(re);
                if (match.groups && match.groups.t) return new PickleToken(type, match.groups.t);
                return this.nextToken();
            }
        }
        // Try special literal strings, quoted and block
        if (this.testRE(/^{/)) {
            var j = 0, depth = 0, string = "";
            do {
                var ch = this.peek(j);
                if (ch == undefined) {
                    // Reached unexpected EOF
                    this.i++;
                    return new PickleToken("error", this.peek(-1));
                }
                if (ch == "{") depth++;
                else if (ch == "}") depth--;
                string += ch;
                j++;
            } while (depth > 0);
            this.i += j;
            return new PickleToken("string.curly", string.slice(1, -1));
        }
        else if (this.testRE(/^:\s*?\n/)) {
            this.i++;
            return new PickleToken("foobar", this.peek(-1));
        }
        this.i++;
        return new PickleToken("error", this.peek(-1));
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