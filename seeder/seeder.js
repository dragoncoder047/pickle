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

output("Tokens appear here");

// function mystringify(x) {
//     if (x === undefined) return "UNDEFINED";
//     if (x === null) return "NULL";
//     return x.toString().replaceAll("\n", "\\n");
// }

// console.debug = function patched(...args) {
//     output(`<span class="debug">${args.map(mystringify).join(" ")}</span>\n`);
// }

var tokenizer = null;
var annotations = [];
var animationID = undefined;
var gotErrors = false;
function tokenizeAnimation() {
    try {
        if (!tokenizer.done()) {
            var tok = tokenizer.nextToken();
            if (tok) {
                if (tok.type == "error") {
                    gotErrors = true;
                    annotations.push({
                        row: tok.start.line - 1,
                        column: tok.start.col,
                        text: tok.message + (tok.content ? `: ${tok.content}` : ""),
                        type: "error",
                    });
                    editor.getSession().setAnnotations(annotations);
                }
                output(`[${tok.start.line}:${tok.start.col} - ${tok.end.line}:${tok.end.col}]\t${tok.type} ${tok.subtypes ? "(" + tok.subtypes.join(",") + ")": ""}\t${JSON.stringify(tok.content)}\t${tok.message}\n`);
            }
            animationID = requestAnimationFrame(tokenizeAnimation);
        } else {
            if (!gotErrors) {
                editor.getSession().setAnnotations([]);
                annotations = [];
            }
        }
    } catch (e) {
        output(`<span class="outerror">${e}\n${e.stack}</span>`)
        console.error(e);
    }
}

editor.getSession().on('change', () => {
    if (animationID) cancelAnimationFrame(animationID);
    tokenizer = new PickleTokenizer(editor.getValue());
    annotations = [];
    gotErrors = false;
    clearOutput();
    tokenizeAnimation();
});