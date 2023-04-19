const $ = s => document.querySelector(s);

ace.config.set('basePath', 'https://cdn.jsdelivr.net/npm/ace-builds@1.10.0/src-noconflict/');
var editor = ace.edit("picklecode");

function output(x) {
    $("#picklelog").innerHTML += x;
}

// output("Test output appears here");
