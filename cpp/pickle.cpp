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

loc::loc(size_t line, size_t col)
: line(line),
  col(col) {};

token::token(token_type type, char* content, loc start, loc end, char* filename, char* message)
: type(type),
  content(content ? strdup(content) : NULL),
  start(start),
  end(end),
  filename(filename ? strdup(filename) : NULL),
  message(message ? strdup(message) : NULL) {};

token::~token() {
    free(this->content);
    free(this->filename);
    free(this->message);
}

tokenizer::tokenizer(const char* stream, const char* filename)
: filename((char*)filename),
  stream((char*)stream),
  len(strlen(stream)),
  bi(0),
  i(0),
  last_token(NULL),
  buffer(NULL),
  bufsz(0),
  buflen(0) {};


token* tokenizer::error_token(char* message) {
    if (this->bi == this->i) this->i++;
    char* message2;
    if (message) message2 = strdup(message);
    else {
        char c = this->at(-1);
        asprintf(&message2, "unexpected '%s%c'", needs_escape(c) ? "\\" : "", escape(c));
    }
    token* foo = this->make_token(ERROR, message2);
    free(message2);
    return foo;
}

}
