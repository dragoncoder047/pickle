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

class PickleParser {
    constructor(code) {
        this.code = code;
        this.i = 0;
    }
    at() {
        return this.code[this.i];
    }
    advance(amount) {
        this.i += amount;
    }
    next() {
        this.advance(1);
    }
    save() {
        return this.i;
    }
    restore(i) {
        this.i = i;
    }
    eof() {
        return this.i >= this.code.length;
    }
    remaining() {
        return this.code.slice(this.i);
    }
    eol() {
        return this.eof() || EOL_CHARS.indexOf(this.at()) != -1;
    }
    startsWith(string) {
        return this.remaining().startsWith(string);
    }
    nextToken(predicate) {
        var out = "";
        while (predicate(this.at())) {
            out += this.at();
            this.next();
        }
        return out;
    }
    skipWhitespaceAndComments() {
        while (true) {
            var start = this.save();
            while (!this.eof()) {
                var ch = this.at();
                if (ch == "#") {
                    if (this.startsWith("###")) {
                        this.advance(2);
                        while (!this.eof() && !this.startsWith("###")) this.next();
                        this.advance(3);
                    } else {
                        while (!this.eol()) this.next();
                    }
                }
                else if (this.eol()) break;
                else if (/\s/.test(ch)) this.next();
                else break;
            }
            if (this.i == start) break;
        }
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

function pickleParse(string) {

}
