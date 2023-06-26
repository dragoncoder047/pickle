#include "pickle.hpp"

namespace pickle {

char unescape(char c) {
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
        case '\n': return 0;
        default: return c;
    }
}

char escape(char c) {
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

bool needs_escape(char c) {
    return strchr("{}\b\t\n\v\f\r\a\\\"", c) != NULL;
}

location::location()
: location(1, 1) {}

location::location(size_t line, size_t col)
: line(line),
  col(col) {}

token::token(token_type type, char* content, location start, location end, char* filename, const char* message)
: type(type),
  content(content ? strdup(content) : NULL),
  start(start),
  end(end),
  filename(filename ? strdup(filename) : NULL),
  message(message ? strdup(message) : NULL) {}

token::~token() {
    free(this->content);
    free(this->filename);
    free(this->message);
}

tokenizer::tokenizer(const char* stream, const char* filename, location offset)
: offset(offset),
  filename((char*)filename),
  stream((char*)stream),
  len(strlen(stream)),
  bi(0),
  i(0),
  last_token(NULL),
  buffer(NULL),
  bufsz(0),
  buflen(0) {}

tokenizer::~tokenizer() {
    free(this->buffer);
}

char tokenizer::at(ssize_t i) {
    ssize_t off = i + this->i;
    if (off < 0 || off >= this->len) return EOF;
    return this->stream[off];
}

char* tokenizer::str_at(ssize_t i) {
    ssize_t off = i + this->i;
    if (off < 0 || off >= this->len) return NULL;
    return &this->stream[off];
}

[[noreturn]] void tokenizer::error(char* offending, const char* message) {
    if (this->bi == this->i) this->i++;
    char m2[17];
    if (!message) {
        char c = this->at(-1);
        asprintf((char**)&m2, "unexpected '%s%c'", needs_escape(c) ? "\\" : "", escape(c));
        message = (const char*) m2;
    }
    this->got_token(ERROR, offending, message, SIGNIFICANT);
}

[[noreturn]] void tokenizer::got_token(token_type type, char* content, const char* message, significance significant, bool free_content) {
    if (content == NULL) {
        int len = this->i - this->bi;
        asprintf(&content, "%.*s", len, &this->stream[this->bi]);
    }
    location here, start;
    for (size_t i = 0; i < this->i; i++) {
        char c = this->stream[i];
        if (i == this->bi) start = here;
        if (c == '\n') {
            here.line++;
            here.col = 1;
        } else {
            here.col++;
        }
    }
    here.line += this->offset.line;
    here.col += this->offset.col;
    start.line += this->offset.line;
    start.col += this->offset.col;
    this->last_token = new token(type, content, start, here, this->filename, message);
    if (content == this->buffer) free(this->buffer), this->bufsz = this->buflen = 0, this->buffer = NULL;
    else if (free_content && content) free(content);
    longjmp(this->success, (int)significant);
}

size_t tokenizer::save() {
    return this->i;
}

void tokenizer::restore(size_t where) {
    this->i = where;
}

#define CHUNK_SZ 16
void tokenizer::append_to_buffer(char c) {
    this->buflen++;
    if (this->buflen >= this->bufsz) {
        this->bufsz += CHUNK_SZ;
        this->buffer = (char*)realloc(this->buffer, this->bufsz + 1);
    }
    this->buffer[this->buflen - 1] = c;
    this->buffer[this->buflen] = 0;
}

bool tokenizer::test_str(char* what) {
    size_t len = strlen(what);
    bool matched = !strncmp(this->str_at(), what, len);
    if (matched) this->advance(len);
    return matched;
}

bool tokenizer::test_any(char* what) {
    bool matched = strchr(what, this->at()) != NULL;
    if (matched) this->advance();
    return matched;
}

bool tokenizer::test(char what) {
    bool matched = this->at() == what;
    if (matched) this->advance();
    return matched;
}

bool tokenizer::test(bool (*match)(char)) {
    bool matched = match(this->at());
    if (matched) this->advance();
    return matched;
}

bool tokenizer::test(int (*match)(int)) {
    bool matched = match(this->at());
    if (matched) this->advance();
    return matched;
}

bool tokenizer::done() {
    return this->i >= this->len;
}

void tokenizer::advance(ssize_t i) {
    size_t newi = this->i + i;
    if (newi > 0 && newi <= this->len) this->i = newi;
}

void tokenizer::try_colon_block() {
    size_t start = this->save();
    // Abort if there is no colon
    if (!this->test(':')) return;
    while (this->test([] (char what) -> bool { return isspace(what) && what != '\n'; }));
    if (this->done() || this->at() != '\n') { this->restore(start); return; }
    // Get the indent
    size_t in_start = this->save();
    int in_len = 0;
    while (this->test(isspace)) in_len++;
    // Make sure there actually is an indent
    if (!len) this->error(NULL, "expected indent after colon+newline");
    char* indent;
    asprintf(&indent, "%.*s", in_len, &this->stream[in_start]);
    // Ensure indent is all the same character
    char ex_indent = indent[0];
    for (size_t i = 0; i < in_len; i++) if (indent[i] != ex_indent) this->error(indent, "mix of tabs and spaces indenting block");
    // Get the lines
    while (true) {
        // Get one line
        do this->append_to_buffer(this->at()), this->advance(); while (this->at() != '\n');
        // At beg of next line: check to see if there is an unindent
        if (!this->test_str(indent)) {
            if (isspace(this->at())) this->error(NULL, "unindent does not match previous indent");
            break;
        }
    }
    this->got_token(STRING, this->buffer);
}

}
