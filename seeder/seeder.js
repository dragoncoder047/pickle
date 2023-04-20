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

editor.getSession().on('change', () => {
    clearOutput();
    try {
        var text = editor.getValue();
        var t = new PickleTokenizer(text);
        while (!t.done()) {
            var tok = t.nextToken();
            if (!tok) continue;
            output(`[${tok.start.line}:${tok.start.col} - ${tok.end.line}:${tok.end.col}]\t${[tok.type].concat(tok.subtypes).join(".")}\t${JSON.stringify(tok.content)}\t${tok.message}\n`);
        }
    } catch (e) {
        output(`<span class="outerror">${e}\n${e.stack}</span>`)
        console.error(e);
    }
});