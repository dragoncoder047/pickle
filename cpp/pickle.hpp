#pragma once

#include "tinobsy/tinobsy.hpp"
#include <cstdlib>
#include <cctype>
#include <cstdint>
#include <cstring>
#include <csetjmp>

namespace pickle {

char escape(char c);
char unescape(char c);
bool needs_escape(char c);

class loc {
    loc(size_t line, size_t col);
    public:
    size_t line;
    size_t col;
    friend class tokenizer;
    friend class token;
};

typedef enum {
    ERROR,
    STRING,
    PAREN,
    EOL,
    SYMBOL
} token_type;

class token {
    token(token_type type, char* content, loc start, loc end, char* filename, char* message);

    public:
    token_type type;
    char* content;
    loc start;
    loc end;
    char* filename;
    char* message;

    ~token();

    friend class tokenizer;
};

// Tokenizer
class tokenizer {
    size_t bi;
    size_t i;
    size_t len;
    token* last_token;
    char* buffer;
    size_t bufsz;
    size_t buflen;
    void append_to_buffer(char c);

    token* error_token(char* message = NULL);
    token* make_token(token_type type, char* message);
    loc current_loc();
    bool test_str(char* what);
    bool test_any(char* what);
    bool test(char what);
    bool done();
    void advance(ssize_t i = 1);
    char at(ssize_t i = 0);
    char* str_at(ssize_t i = 0);

    bool try_colon_block();
    bool try_block_comment();
    bool try_line_comment();
    bool try_paren();
    bool try_space();
    bool try_eol();
    bool try_symbol();
    bool try_curly_string();
    bool try_quote_string();

    public:
    char* filename;
    char* stream;

    tokenizer(const char* stream, const char* filename = "");
    ~tokenizer();

    token* next_token();
};



}

#include "pickle.cpp"
