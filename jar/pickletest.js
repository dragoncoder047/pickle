const $ = s => document.querySelector(s);

ace.config.set('basePath', 'https://cdn.jsdelivr.net/npm/ace-builds@1.10.0/src-noconflict/');
var editor = ace.edit("textarea");

function output(x) {
    $("#output").innerHTML += x;
    alert(x);
}

output("testing 123");
