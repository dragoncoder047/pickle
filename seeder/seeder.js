const $ = s => document.querySelector(s);

ace.config.set('basePath', 'https://cdn.jsdelivr.net/npm/ace-builds@1.10.0/src-noconflict/');
var editor = ace.edit("picklecode");
const Range = ace.require("ace/range").Range;

function output(x) {
    $("#picklelog").innerHTML += x;
}

function clearOutput() {
    $("#picklelog").innerHTML = "";
}

// function mystringify(x) {
//     if (x === undefined) return "UNDEFINED";
//     if (x === null) return "NULL";
//     return x.toString().replaceAll("\n", "\\n");
// }

// console.debug = function patched(...args) {
//     output(`<span class="debug">${args.map(mystringify).join(" ")}</span>\n`);
// }


function foobar() {
    var tokenizer = new PickleTokenizer(editor.getValue());
    var annotations = [];
    clearOutput();
    try {
        while (!tokenizer.done()) {
            var oldi = tokenizer.i;
            var tok = tokenizer.nextToken();
            if (tok) {
                if (tok.type == "error") {
                    annotations.push({
                        row: tok.start.line - 1,
                        column: tok.start.col,
                        text: tok.message + (tok.content ? `: ${tok.content}` : ""),
                        type: "error",
                    });
                }
                output(`[${tok.start.line}:${tok.start.col} - ${tok.end.line}:${tok.end.col}]\t${tok.type} ${tok.subtypes.length > 0 ? "(" + tok.subtypes.join(",") + ")" : ""}\t${JSON.stringify(tok.content)}\t${tok.message}\n`);
            }
            if (tokenizer.i == oldi) throw new Error("Tokenizer error");
        }
    } catch (e) {
        output(`<span class="outerror">${e}\n${e.stack}</span>`)
        console.error(e);
    }
    editor.getSession().setAnnotations(annotations);
}

editor.getSession().on('change', foobar);
foobar();
