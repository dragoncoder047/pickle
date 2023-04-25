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
            while (true) {
                var line = this.chomp(/^[^\n]*/);
                lines.push(line[0] || "");
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
            { type: "comment.block", re: /^(?<!#)(###\S*?###)(?!#)[\s\S\n\r]*?(?<!#)\1(?!#)/, significant: false },
            { type: "comment.line", re: /^##[^\n]*/, significant: false },
            { type: "paren", re: /^[\(\)\[\]]/, significant: true, groupNum: 0 },
            { type: "space", re: /^(?!\n)\s+/, significant: false },
            { type: "eol", re: /^[;\n]+/, significant: true, groupNum: 0 },
            { type: "singleton", re: /^(true|false|nil)/, significant: true, groupNum: 0 },
            { type: "number.complex", re: /^-?[0-9]+(\.[0-9]+)?e[+-]\d+[+-][0-9]+(\.[0-9]+)?e[+-]\d+j/, significant: true, groupNum: 0 },
            { type: "number.rational", re: /^-?[0-9]+\/[0-9]+/, significant: true, groupNum: 0 },
            { type: "number.integer", re: /^-?([1-9][0-9]*|0x[0-9a-f]+|0b[01]+)/i, significant: true, groupNum: 0 },
            { type: "number.float", re: /^-?[0-9]+(\.[0-9]+)?(e[+-]\d+)?/i, significant: true, groupNum: 0 },
            { type: "symbol", re: /^[a-z_][a-z0-9_]*\??/i, significant: true, groupNum: 0 },
            { type: "symbol.operator", re: /^[-~`!@#$%^&*_+=[\]|\\:<>,.?/]+/, significant: true, groupNum: 0 },
        ]
        for (var { type, re, significant, groupNum } of TOKEN_REGEXES) {
            if (this.test(re)) {
                var match = this.chomp(re);
                if (significant) return this.makeToken(type, match[groupNum]);
                else return this.nextToken();
            }
        }
        // Try strings
        if (this.test("{")) {
            var j = 0, depth = 0, string = "";
            do {
                var ch = this.peek(j);
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
            var q = this.chomp(/^['"]/)[0];
            var j = 0, string = "";
            while (true) {
                var ch = this.peek(j);
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

class PickleOperator {
    /**
     * @type {Map<string, PickleOperator>}
     */
    static _interned = new Map();
    /**
     * Create a new operator data.
     * @param {string} symbol
     * @param {"prefix" | "right" | "left" | "postfix"} associativity
     * @param {number?} precedence
     */
    constructor(symbol, associativity, precedence = 1) {
        /**
         * @type {string}
         */
        this.symbol = symbol;
        /**
         * @type {"prefix" | "right" | "left" | "postfix"}
         */
        this.associativity = associativity;
        /**
         * @type {number}
         */
        this.precedence = precedence;
        if (PickleOperator._interned.has(this.toString())) return PickleOperator._interned.get(this.toString());
        PickleOperator._interned.set(this.toString(), this);
    }
    toString() {
        return JSON.stringify({
            symbol: this.symbol,
            associativity: this.associativity,
            precedence: this.precedence
        });
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
    static typeName = "object";
    /**
     * Create a new Pickle object with the specified prototypes.
     * @param {...PickleObject} prototypes
     */
    constructor(...prototypes) {
        /**
         * @type {Map<string, PickleObject>}
         */
        this.properties = new Map();
        /**
         * @type {Map<PickleOperator, PickleObject>}
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
    get typeName() {
        return this.constructor.typeName;
    }
    /**
     * Returns the method resolution order for this object's multiprototype tree.
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
    /**
     * Call the object with the specified parameters.
     * @param {PickleCollection} args
     * @param {PickleScope} scope
     * @param {PickleObject?} thisArg
     * @returns {PickleObject}
     */
    call(args, scope, thisArg = this) {
        if (args.length > 0) throw new PickleError(`can't call ${this.typeName}`);
        return thisArg;
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
     */
    constructor(sym) {
        super(PickleSymbolPrototype);
        if (PickleSymbol._interned.has(sym)) return PickleSymbol._interned.get(sym);
        /**
         * @type {string}
         */
        this.sym = sym;
        PickleSymbol._interned.set(sym, this);
    }
    get length() {
        return this.sym.length;
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
     */
    constructor(str) {
        super(PickleStringPrototype);
        if (PickleString._interned.has(str)) return PickleString._interned.get(str);
        /**
         * @type {string}
         */
        this.str = str;
        PickleString._interned.set(str, this);
    }
    get length() {
        return this.str.length;
    }
}

class PickleScalar extends PickleObject { }

class PickleFloat extends PickleScalar {
    static typeName = "float";
    /**
     * @type {Map<number, PickleFloat>}
     */
    static _interned = new Map();
    /**
     * @param {number} num The number
     */
    constructor(num) {
        super(FAIL);
        if (PickleFloat._interned.has(num)) return PickleFloat._interned.get(num);
        /**
         * @type {number}
         */
        this.num = num;
        PickleFloat._interned.set(num, this);
    }
}

class PickleComplex extends PickleObject {
    static typeName = "complex";
    constructor(...x) {
        throw new PickleError('not supported yet');
    }
}

class PickleRational extends PickleObject {
    static typeName = "rational";
    constructor(...x) {
        throw new PickleError('not supported yet');
    }
}

class PickleInteger extends PickleScalar {
    static typeName = "integer";
    /**
     * @type {Map<BigInt, PickleInteger>}
     */
    static _interned = new Map();
    /**
     * @param {BigInt} num The number
     */
    constructor(num) {
        super(FAIL);
        if (PickleInteger._interned.has(num)) return PickleInteger._interned.get(num);
        /**
         * @type {BigInt}
         */
        this.num = num;
        PickleInteger._interned.set(num, this);
    }
}

class PickleBoolean extends PickleInteger {
    static typeName = "boolean";
    /**
     * @type {Map<boolean, PickleBoolean>}
     */
    static _interned = new Map();
    /**
     * @param {boolean} num The number
     */
    constructor(b) {
        super(FAIL);
        if (PickleBoolean._interned.has(num)) return PickleBoolean._interned.get(num);
        /**
         * @type {boolean}
         */
        this.b = b;
        PickleBoolean._interned.set(b, this);
    }
}

class PickleErrorObject extends PickleObject {
    static typeName = "error";
    /**
     * @param {PickleError} error The thrown error
     */
    constructor(error) {
        super(FAIL);
        /**
         * @type {PickleError}
         */
        this.error = error;
    }
}

class PickleFunction extends PickleObject {
    static typeName = "function";
    /**
     * @param {PickleCollection} args The signature
     * @param {PickleString} body The code of the function
     */
    constructor(args, body) {
        super(PickleFunctionPrototype);
        /**
         * @type {PickleCollection}
         */
        this.arguments = args;
        /**
         * @type {PickleString}
         */
        this.body = body;
    }
}

/**
 * @typedef {(args: PickleObject[], scope: PickleScope) => PickleObject} PickleBuiltinFunctionCallback
 */

class PickleBuiltinFunction extends PickleObject {
    static typeName = "builtin_function";
    /**
     * @param {PickleBuiltinFunctionCallback} fun The callable interface
     */
    constructor(fun) {
        super(PickleFunctionPrototype);
        /**
         * @type {PickleBuiltinFunctionCallback}
         */
        this.fun = fun;
    }

    call(args, scope) {
        return this.fun(args, scope);
    }
}

class PickleStream extends PickleObject {
    static typeName = "stream";
    /**
     * @param {stream} stream The stream
     */
    constructor(stream) {
        super(FAIL);
        /**
         * @type {stream}
         */
        this.stream = stream;
    }
}

class PickleCollectionEntry extends PickleObject {
    static typeName = "colle_entryction";
    /**
     * @param {PickleObject?} key The key value
     * @param {PickleObject} value The value at the key
     */
    constructor(key, value) {
        super(FAIL);
        /**
         * @type {PickleObject}
         */
        this.key = key;
        /**
         * @type {PickleObject}
         */
        this.value = value;
    }
}

class PickleCollection extends PickleObject {
    static typeName = "collection";
    /**
     * @param {PickleCollectionEntry[]} items The entries
     */
    constructor(items) {
        super(FAIL);
        /**
         * @type {PickleCollectionEntry[]}
         */
        this.items = items;
    }
    /**
     * Get the item at this index
     * @param {number} index 
     * @returns {PickleObject}
     */
    get(index) {
        return this.items[index].value;
    }
    /**
     * Finds the value for this key.
     * @param {PickleObject} key
     * @returns {PickleObject?}
     */
    find(key) {
        for (var kvp of this.items) {
            if (kvp.key === key) return kvp.value;
        }
        return null;
    }
}

class PickleInternalObject extends PickleObject { }

class PickleExpression extends PickleInternalObject {
    static typeName = "expression";
    /**
     * @param {PickleObject[]} items The items to be evaluated
     */
    constructor(items) {
        super(FAIL);
        /**
         * @type {PickleObject[]}
         */
        this.items = items;
    }
}

class PickleScope extends PickleInternalObject {
    static typeName = "scope";
    /**
     * @param {PickleCollection} bindings The bindings
     */
    constructor(bindings) {
        super(FAIL);
        /**
         * @type {PickleCollection}
         */
        this.bindings = bindings;
        /**
         * @type {PickleObject}
         */
        this.returnValue = null;
        /**
         * @type {PickleObject}
         */
        this.lastValue = null;
    }
}

class PickleExpando extends PickleInternalObject {
    static typeName = "expando";
    /**
     * @param {PickleCollection} expanded The expanded items
     */
    constructor(expanded) {
        super(FAIL);
        /**
         * @type {PickleCollection}
         */
        this.expanded = expanded;
    }
}

class PickleBoundProperty extends PickleInternalObject {
    static typeName = "bound_property_or_function";
    /**
     * @param {PickleObject} thisArg The object bound to
     * @param {PickleSymbol} property The property key
     */
    constructor(thisArg, property) {
        super(FAIL);
        /**
         * @type {PickleObject}
         */
        this.thisArg = thisArg;
        /**
         * @type {PickleSymbol}
         */
        this.property = property;
    }
}

class PicklePropertySetter extends PickleInternalObject {
    static typeName = "setter";
    /**
     * @param {PickleSymbol} property The property key
     * @param {PickleObject} value The value to set to
     */
    constructor(property, value) {
        super(FAIL);
        /**
         * @type {PickleSymbol}
         */
        this.property = property;
        /**
         * @type {PickleObject}
         */
        this.value = value;
    }
}

/**
 * @param {string | number | BigInt | PickleBuiltinFunctionCallback} it
 * @returns {PickleObject}
 */
function toPickle(it) {
    switch (typeof it) {
        case "string":
            return new PickleString(it);
        case "number":
            return new PickleFloat(it);
        case "function":
            return new PickleBuiltinFunction(it);
        case "boolean":
            return new PickleBoolean(it);
        default:
            if (it instanceof BigInt) return new PickleInteger(it);
    }
    throw new PickleError(`can't convert ${typeof it}`);
}

/**
 * Create an object with the specified properties.
 * @param {...PickleObject} prototypes
 * @param {{properties: string | number | BigInt | function, operators: [PickleOperator, string | number | BigInt | function][]}} data 
 */
function newPickleObject() {
    /**
     * @type {PickleObject[]}
     */
    var prototypes = [].slice.call(arguments);
    /**
     * @type {{properties: string | number | BigInt | function, operators: [PickleOperator, string | number | BigInt | function][]}}
     */
    var data = prototypes.pop();
    var o = new PickleObject(...prototypes);
    if (data.properties) {
        for (var pname of Object.getOwnPropertyNames(data.properties)) {
            o.properties.set(pname, toPickle(data.properties[pname]));
        }
    }
    if (data.operators) {
        for (var [op, payload] of data.operators) {
            o.operators.set(op, toPickle(payload));
        }
    }
    return o;
}

/**
 * @type {PickleObject}
 */
var PickleFunctionPrototype = {};

const PickleObjectPrototype = newPickleObject({
    operators: [
        [new PickleOperator(".", "left", 100), function (args, scope) {
            var x, prop = args.get(0);
            for (var p of this.getMRO())
                if ((x = p.properties.find(prop)))
                    return new PickleBoundProperty(this, x);
            throw new PickleError(`no ${p.sym} in ${this.typeName} object`);
        }],
        [new PickleOperator("==", "left", 2), function (args, scope) {
            return new PickleBoolean(this === args.get(0));
        }]
    ]
});

var foobar = newPickleObject(PickleObjectPrototype, {
    operators: [
        [new PickleOperator("|>", "right", 2), function (args, scope) {
            return this.call(args.get(0));
        }],
    ]
});
// reference loop kludge
Object.assign(PickleFunctionPrototype, foobar);
Object.setPrototypeOf(PickleFunctionPrototype, PickleObject);

const PickleSymbolPrototype = newPickleObject(PickleObjectPrototype, {
    operators: [
        [new PickleOperator("$", "prefix", 100), function (args, scope) {
            var x;
            for (var s of scope.getMRO())
                if (s instanceof PickleScope && (x = s.bindings.find(this.sym)))
                    return x;
            throw new PickleError(`undefined variable ${this.sym}`);
        }],
        [new PickleOperator("=", "left", -1), function (args, scope) {
            return new PicklePropertySetter(this, args.get(0));
        }],
    ]
});

const PickleStringPrototype = newPickleObject(PickleObjectPrototype, {
    operators: [
        [new PickleOperator("+", "left", 3), function (args, scope) {
            var other = args.get(0);
            if (!(other instanceof PickleString)) throw new PickleError(`can't add string to ${other.typeName}`);
            return new PickleString(this.str + other.str);
        }]
        [new PickleOperator("*", "left", 4), function (args, scope) {
            var other = args.get(0);
            if (!(other instanceof PickleScalar)) throw new PickleError(`can't repeat string by ${other.typeName}`);
            return new PickleString(this.str.repeat(other.num));
        }]
    ]
});

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
        this.tokenizer = new PickleTokenizer(code, filename);
        /**
         * @type {string}
         */
        this.filename = filename;
    }
}
