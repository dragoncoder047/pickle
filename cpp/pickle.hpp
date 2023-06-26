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

// A struct to hold the line/column information for tokens.
class location {
    public:
    location();
    location(size_t line, size_t col);
    size_t line;
    size_t col;
};

// What kind of token it is.
typedef enum {
    ERROR,
    STRING,
    PAREN,
    EOL,
    SYMBOL,
} token_type;

// A struct to hold the data for tokens.
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

// Whether the token means anything to the parser or not. If it doesn't, the token is dropped.
typedef enum {
    SIGNIFICANT = 1,
    INSIGNIFICANT = 2
} significance;

// Tokenizer
class tokenizer {
    // If not line 1, col 1, where in the file parsing started. (Used for nested blocks).
    location offset;
    // Begin index
    size_t bi;
    // Current index
    size_t i;
    // Total length of stream to be parsed
    size_t len;
    // Last token successfully parsed
    token* last_token;
    // Buffer for string data not taken directly from the source stream.
    char* buffer;
    // Capacity of the buffer.
    size_t bufsz;
    // Number of characters in the buffer.
    size_t buflen;
    // Append the character to the buffer, enlarging the buffer if needed.
    void append_to_buffer(char c);

    // Used to longjmp back to the top when a token is successfully parsed.
    jmp_buf success;

    // Returns the current index.
    size_t save();
    // Restores the current index to what it was previously.
    void restore(size_t where);

    // Signals an error-token.
    [[noreturn]] void error(char* offending, const char* message);

    // Signals a good token.
    [[noreturn]] void got_token(token_type type, char* content, const char* message = NULL, significance significant = SIGNIFICANT, bool free_content = true);

    // If the current position begins with `what`, advances and returns true, otherwise returns false.
    bool test_str(char* what);
    // If the current position begins with `what`, advances and returns true, otherwise returns false.
    bool test_any(char* what);
    // If the current position begins with `what`, advances and returns true, otherwise returns false.
    bool test(char what);
    // If `match` returns true for the current character, advances and returns true, otherwise returns false.
    bool test(bool (*match)(char));
    // If `match` returns true for the current character, advances and returns true, otherwise returns false.
    bool test(int (*match)(int));
    // True if all the characters in the stream have been parsed.
    bool done();
    // Advances the pointer by `i` characters.
    void advance(ssize_t i = 1);
    // Returns the character at the current pointer +- an offset.
    char at(ssize_t i = 0);
    // Returns the remainder of the string starting at the current pointer +- an offset.
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
    // The file that the stream originated from.
    char* filename;
    // The contents to be parsed.
    char* stream;

    tokenizer(const char* stream, const char* filename = "", location offset = {0, 0});
    ~tokenizer();

    token* next_token();
};



}

#include "pickle.cpp"
