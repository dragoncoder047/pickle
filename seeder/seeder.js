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

output("Test output appears here\n");

function highlight(token) {
    editor.getSession().addMarker(
        new Range(token.start.line - 1, token.start.col, token.end.line - 1, token.end.col),
        token.type,
        "text",
        false);
}

function clearHighlights() {
    var prevMarkers = editor.getSession().getMarkers();
    if (prevMarkers) {
        for (var item of Object.keys(prevMarkers)) {
            editor.getSession().removeMarker(prevMarkers[item].id);
        }
    }
}

editor.getSession().on('change', () => {
    clearOutput();
    try {
        var text = editor.getValue();
        var t = new PickleTokenizer(text);
        while (!t.done()) {
            var tok = t.nextToken();
            highlight(tok);
            output(`[${tok.start.line}:${tok.start.col} - ${tok.end.line}:${tok.end.col}] ${[tok.type].concat(tok.subtypes).join(".")} ${JSON.stringify(tok.content)}\n`);
        }
    } catch (e) {
        output(`<span class="error">${e}\n${e.stack}</span>`)
        console.error(e);
    }
});