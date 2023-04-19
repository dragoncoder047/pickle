const $ = s => document.querySelector(s);

ace.config.set('basePath', 'https://cdn.jsdelivr.net/npm/ace-builds@1.10.0/src-noconflict/');
var editor = ace.edit("picklecode");

function output(x) {
    $("#picklelog").innerHTML += x;
}

function clearOutput() {
    $("#picklelog").innerHTML = "";
}

for (var i = 0; i < 10; i++) output("Test output appears here\n");


editor.getSession().on('change', () => {
    try {
        var text = editor.getValue();
        var t = new PickleTokenizer(text);
        var tokens = [];
        while (!t.empty()) {
            tokens.push(t.nextToken());
        }
        clearOutput();
        output(JSON.stringify(tokens, null, 4));
    } catch (e) {
        output(`<span class="error">${e}\n${e.stack}</span>`)
    }
});