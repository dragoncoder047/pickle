/**
 * Error raised by a Pickle program.
 */
class PickleError extends Error {
    /**
     * @param {string} message Error message
     */
    constructor(message) {
        super(message);
        /**
         * @type {string}
         */
        this.name = this.constructor.name;
    }
}

/**
 * Object representing the location (filename, line, column)
 * a particular object was originally defined in.
 */
class PickleSourceLocation {
    /**
     * @param {string} filename
     * @param {number} line
     * @param {number} col
     */
    constructor(filename, line, col) {
        /**
         * @type {string}
         */
        this.filename = filename;
        /**
         * @type {number}
         */
        this.line = line;
        /**
         * @type {number}
         */
        this.col = col;
    }
    toString() {
        return `File ${this.filename}, line ${this.line}`;
    }
    toJSON() {
        return {
            filename: this.filename,
            line: this.line,
            col: this.col,
        };
    }
}

/**
 * Unescapes a character found immediately after a \\ escape.
 * @param {string} c A character
 * @returns {string}
 */
function pickleUnescapeChar(c) {
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

/**
 * Returns the escape character to be placed after a \\, to escape the character.
 * @param {string} c A character
 * @returns {string}
 */
function pickleEscapeChar(c) {
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

/**
 * Returns true if the character needs to be escaped.
 * @param {string} c A character
 * @returns {boolean}
 */
function pickleNeedsEscape(c) {
    return "{}\b\t\n\v\f\r\a\\\"".indexOf(c) != -1;
}

/**
 * @typedef {Object} LineColumn
 * @prop {number} line
 * @prop {number} col
 */

/**
 * One token in a Pickle parse stream.
 */
class PickleToken {
    /**
     * Create a new `PickleToken`.
     * @param {string} type
     * @param {string} content
     * @param {LineColumn} start
     * @param {LineColumn} end
     * @param {string} [filename=""]
     * @param {string} [message=""]
     */
    constructor(type, content, start, end, filename = "", message = "") {
        var types = type.split(".");
        /**
         * @type {string}
         */
        this.type = types[0];
        /**
         * @type {string[]}
         */
        this.subtypes = types.slice(1);
        /**
         * @type {string}
         */
        this.content = content;
        /**
         * @type {LineColumn}
         */
        this.start = start;
        /**
         * @type {LineColumn}
         */
        this.end = end;
        /**
         * @type {string}
         */
        this.filename = filename;
        /**
         * @type {string}
         */
        this.message = message;
    }
    toJSON() {
        return {
            type: this.type,
            subtypes: this.subtypes,
            content: this.content,
            start: this.start,
            end: this.end,
            filename: this.filename,
            message: this.message
        };
    }
}

/**
 * An object that yields `PickleToken`s from a string.
 */
class PickleTokenizer {
    /**
     * Create a new `PickleTokenizer`.
     * @param {string} string The stream
     * @param {string } filename
     */
    constructor(string, filename) {
        /**
         * @type {string}
         */
        this.string = string;
        /**
         * @type {number}
         */
        this.i = 0;
        /**
         * @type {LineColumn?}
         */
        this.beginning = null;
        /**
         * @type {number}
         */
        this.bi = 0;
        /**
         * @type {string}
         */
        this.filename = filename;
    }
    /**
     * Get the current line and column the tokenizer is sitting on.
     * @returns {LineColumn}
     */
    lineColumn() {
        var before = this.string.slice(0, this.i);
        var doneLines = before.split("\n");
        var line = doneLines.length;
        var col = doneLines.at(-1).length + 1;
        return { line, col };
    }
    /**
     * Test if the current position in the stream starts with 
     * @param {string | RegExp} what The prefix to check.
     * @returns {boolean}
     */
    test(what) {
        if (typeof what === "string") return this.string.slice(this.i).startsWith(what);
        else if (what instanceof RegExp) return what.test(this.string.slice(this.i));
        else return false;
    }
    /**
     * Chomps the prefix off, advances the stream, and returns what was chomped.
     * @param {string | RegExp} what The prefix to chomp.
     * @returns {what instanceof RegExp ? RegExpExecArray : string}
     */
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
    /**
     * Returns true if the tokenizer is empty.
     * @returns {boolean}
     */
    done() {
        return this.i >= this.string.length;
    }
    /**
     * Gets the `i`-th character after where the tokenizer is sitting on. 
     * @param {number} [i=0]
     * @returns {string?}
     */
    peek(i = 0) {
        var j = this.i + i;
        if (j >= this.string.length) return undefined;
        return this.string[j];
    }
    /**
     * Creates an error token, and if the tokenizer has not advanced yet, advances it one character.
     * @param {string} [message=""] 
     * @returns {PickleToken}
     */
    errorToken(message = "") {
        console.debug("errorToken()", message);
        // always advance to allow more tokenizing
        if (this.bi == this.i) this.i++;
        return this.makeToken("error", this.string.slice(this.bi, this.i), message || `unexpected ${this.peek(-1)}`);
    }
    /**
     * Makes a new token, with the beginnning and end parameters filled in.
     * @param {string} type
     * @param {string} content
     * @param {string} [message=""]
     * @returns {PickleToken}
     */
    makeToken(type, content, message = "") {
        return new PickleToken(type, content, this.beginning, this.lineColumn(), this.filename, message);
    }
    /**
     * Advances the tokenizer and returns the next token, or undefined if the tokenizer is empty.
     * @returns {PickleToken?}
     */
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
            { type: "comment.block", re: /^(?<!#)(###+)(?!#)[\s\S\n\r]*?(?<!#)\1(?!#)/, significant: false },
            { type: "comment.line", re: /^#[^\n]*/, significant: false },
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
                    ch = pickleUnescapeChar(this.peek(j + 1));
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


/**
 * All objects in Pickle are instances of this.
 */
class PickleObject {
    /**
     * The printable name of the type of the object.
     * @type {string}
     */
    static typeName = "baseobject";
    /**
     * Create a new Pickle object.
     * @param {...PickleObject} prototypes
     */
    constructor(...prototypes) {
        /**
         * @type {Map<string, PickleObject>}
         */
        this.properties = new Map();
        /**
         * @type {Map<string, PickleObject>}
         */
        this.operators = new Map();
        /**
         * @type {PickleObject[]}
         */
        this.prototypes = prototypes;
        /**
         * @type {PickleSourceLocation?}
         */
        this.source = null;
    }
    /**
     * Returns the method resolution order for this object's prototype chain.
     * @returns {PickleObject[]}
     */
    getMRO() {
        var fun = x => [x].concat(x.prototypes.map(fun));
        return fun(this).flat(Infinity).filter((x, i, a) => a.slice(0, i).indexOf(x) == -1);
    }
    /**
     * Returns true if the object has a property on itself (not a prototype)
     * with the specified name.
     * @param {string} pname Property name
     * @returns {boolean}
     */
    hasOwnProperty(pname) {
        return this.properties.has(pname)
    }
    /**
     * Get the specified property, recursing up the prototype tree.
     * Returns `null` if the property does not exist on this object.
     * @param {string} pname
     * @returns {PickleObject?}
     */
    getProperty(pname) {
        if (this.hasOwnProperty(pname)) return this.properties.get(pname);
        for (var p of this.getMRO()) {
            if (p.hasOwnProperty(pname)) return p.getProperty(pname);
        }
        return null;
    }
    /**
     * Set the property. If `value` is `null`, the property is deleted.
     * @param {string} pname
     * @param {PickleObject?} value
     */
    setProperty(pname, value) {
        throw 'todo';
    }
}

class PickleSymbol extends PickleObject {
    static typeName = "symbol";
    /**
     * @type {Map<string, PickleSymbol>}
     */
    static _interned = new Map();
    /**
     * @param {string} sym The symbol content
     * @param  {...PickleObject} prototypes
     */
    constructor(sym, ...prototypes) {
        super(...prototypes);
        if (PickleSymbol._interned.has(sym)) return PickleSymbol._interned.get(sym);
        /**
         * @type {string}
         */
        this.sym = sym;
        PickleSymbol._interned.set(sym, this);
    }
    toString() {
        return this.sym;
    }
    valueOf() {
        return this.sym;
    }
}

class PickleString extends PickleObject {
    static typeName = "string";
    /**
     * @type {Map<string, PickleString>}
     */
    static _interned = new Map();
    /**
     * @param {string} str The string content
     * @param  {...PickleObject} prototypes
     */
    constructor(str, ...prototypes) {
        super(...prototypes);
        if (PickleString._interned.has(str)) return PickleString._interned.get(str);
        /**
         * @type {string}
         */
        this.str = str;
        PickleString._interned.set(str, this);
    }
    toString() {
        return this.str;
    }
    valueOf() {
        return this.str;
    }
}

class PickleFloat extends PickleObject {
    static typeName = "float";
    /**
     * @type {Map<number, PickleFloat>}
     */
    static _interned = new Map();
    /**
     * @param {number} num The number
     * @param  {...PickleObject} prototypes
     */
    constructor(num, ...prototypes) {
        super(...prototypes);
        if (PickleFloat._interned.has(num)) return PickleFloat._interned.get(num);
        /**
         * @type {number}
         */
        this.num = num;
        PickleFloat._interned.set(num, this);
    }
    toString(base) {
        return this.num.toString(base);
    }
    valueOf() {
        return this.num;
    }
}

class PickleComplex extends PickleObject {
    constructor(...x) {
        throw 'not supported yet';
    }
}

class PickleRational extends PickleObject {
    constructor(...x) {
        throw 'not supported yet';
    }
}

class PickleInteger extends PickleObject {
    static typeName = "integer";
    /**
     * @type {Map<BigInt, PickleInteger>}
     */
    static _interned = new Map();
    /**
     * @param {BigInt} num The number
     * @param  {...PickleObject} prototypes
     */
    constructor(num, ...prototypes) {
        super(...prototypes);
        if (PickleInteger._interned.has(num)) return PickleInteger._interned.get(num);
        /**
         * @type {BigInt}
         */
        this.num = num;
        PickleInteger._interned.set(num, this);
    }
    toString(base) {
        return this.num.toString(base);
    }
    valueOf() {
        return this.num;
    }
}

class PickleErrorObject extends PickleObject {
    static typeName = "error";
    /**
     * @param {PickleError} error The thrown error
     * @param  {...PickleObject} prototypes
     */
    constructor(error, ...prototypes) {
        super(...prototypes);
        /**
         * @type {PickleError}
         */
        this.error = error;
    }
    toString() {
        return this.error.message;
    }
    valueOf() {
        return this.error;
    }
}

class PickleFunction extends PickleObject {
    static typeName = "function";
    /**
     * @param {PickleCollection} args The signature
     * @param {PickleString} body The code of the function
     * @param  {...PickleObject} prototypes
     */
    constructor(args, body, ...prototypes) {
        super(...prototypes);
        /**
         * @type {PickleCollection}
         */
        this.arguments = args;
        /**
         * @type {PickleString}
         */
        this.body = body;
    }
    toString() {
        return this.body.toString();
    }
    valueOf() {
        return this.body;
    }
}

class PickleBuiltinFunction extends PickleObject {
    static typeName = "builtin_function";
    /**
     * @param {(args: PickleObject[]) => PickleObject} fun The callable interface
     * @param  {...PickleObject} prototypes
     */
    constructor(fun, ...prototypes) {
        super(...prototypes);
        /**
         * @type {(args: PickleObject[]) => PickleObject}
         */
        this.fun = fun;
    }
    toString() {
        return this.fun.toString();
    }
    valueOf() {
        return this.fun;
    }
}

class PickleStream extends PickleObject {
    static typeName = "stream";
    /**
     * @param {stream} s The stream
     * @param  {...PickleObject} prototypes
     */
    constructor(s, ...prototypes) {
        super(...prototypes);
        /**
         * @type {stream}
         */
        this.stream = stream;
    }
    toString() {
        return this.stream.toString();
    }
    valueOf() {
        return this.stream;
    }
}

class PickleCollectionEntry extends PickleObject {
    static typeName = "colle_entryction";
    /**
     * @param {PickleObject?} key The key value
     * @param {PickleObject} value The value at the key
     * @param  {...PickleObject} prototypes
     */
    constructor(key, value, ...prototypes) {
        super(...prototypes);
        /**
         * @type {PickleObject}
         */
        this.key = key;
        /**
         * @type {PickleObject}
         */
        this.value = value;
    }
    toString() {
        return this.value.toString();
    }
    valueOf() {
        return this.value;
    }
}

class PickleCollection extends PickleObject {
    static typeName = "collection";
    /**
     * @param {PickleCollectionEntry[]} items The symbol content
     * @param  {...PickleObject} prototypes
     */
    constructor(items, ...prototypes) {
        super(...prototypes);
        /**
         * @type {PickleCollectionEntry[]}
         */
        this.items = items;
    }
    toString() {
        return this.items.toString();
    }
    valueOf() {
        return this.items;
    }
}

/**
 * An object that parses Pickle code into an abstract syntax tree.
 */
class PickleParser {
    /**
     * Create a new parser.
     * @param {string} code
     * @param {string} filename
     */
    constructor(code, filename) {
        /**
         * @type {PickleTokenizer}
         */
        this.tokenizer = new PickleTokenizer(code);
        /**
         * @type {string}
         */
        this.filename = filename;
    }
}
