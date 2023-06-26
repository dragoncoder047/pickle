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

class location {
    public:
    location();
    location(size_t line, size_t col);
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
    SYMBOL,
} token_type;

class token {
    token(token_type type, char* content, location start, location end, char* filename, const char* message);

    public:
    token_type type;
    char* content;
    location start;
    location end;
    char* filename;
    char* message;

    ~token();

    friend class tokenizer;
};


typedef enum {
    SIGNIFICANT = 1,
    INSIGNIFICANT = 2
} significance;

// Tokenizer
class tokenizer {
    location offset;
    size_t bi;
    size_t i;
    size_t len;
    token* last_token;
    char* buffer;
    size_t bufsz;
    size_t buflen;
    void append_to_buffer(char c);

    jmp_buf success;

    size_t save();
    void restore(size_t where);

    [[noreturn]] void error(char* offending, const char* message);
    [[noreturn]] void got_token(token_type type, char* content, const char* message = NULL, significance significant = SIGNIFICANT, bool free_content = true);

    bool test_str(char* what);
    bool test_any(char* what);
    bool test(char what);
    bool test(bool (*match)(char));
    bool test(int (*match)(int));
    bool done();
    void advance(ssize_t i = 1);
    char at(ssize_t i = 0);
    char* str_at(ssize_t i = 0);

    void try_colon_block();
    void try_block_comment();
    void try_line_comment();
    void try_paren();
    void try_space();
    void try_eol();
    void try_symbol();
    void try_curly_string();
    void try_quote_string();

    public:
    char* filename;
    char* stream;

    tokenizer(const char* stream, const char* filename = "", location offset = {0, 0});
    ~tokenizer();

    token* next_token();
};



}

#include "pickle.cpp"
