/*
 * MIT License
 *
 * Copyright (c) 2010 Serge Zaitsev
 *
 * Permission is hereby granted, free of charge, to any person obtaining a copy
 * of this software and associated documentation files (the "Software"), to deal
 * in the Software without restriction, including without limitation the rights
 * to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
 * copies of the Software, and to permit persons to whom the Software is
 * furnished to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice shall be included in
 * all copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
 * AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 * LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
 * OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
 * SOFTWARE.
 *
 * Source: https://github.com/zserge/jsmn (upstream master, retrieved 2026-08-20)
 * The local copy retains the allocation-free tokenizer design and is formatted
 * for this project's strict C11 build.
 */
#ifndef JSMN_H
#define JSMN_H

#include <stddef.h>

#ifdef JSMN_STATIC
#define JSMN_API static
#else
#define JSMN_API extern
#endif

typedef enum {
    JSMN_UNDEFINED = 0,
    JSMN_OBJECT = 1 << 0,
    JSMN_ARRAY = 1 << 1,
    JSMN_STRING = 1 << 2,
    JSMN_PRIMITIVE = 1 << 3,
} jsmntype_t;

enum jsmnerr {
    JSMN_ERROR_NOMEM = -1,
    JSMN_ERROR_INVAL = -2,
    JSMN_ERROR_PART = -3,
};

typedef struct jsmntok {
    jsmntype_t type;
    int start;
    int end;
    int size;
    int parent;
} jsmntok_t;

typedef struct jsmn_parser {
    unsigned int pos;
    unsigned int toknext;
    int toksuper;
} jsmn_parser;

JSMN_API void jsmn_init(jsmn_parser *parser);
JSMN_API int jsmn_parse(
    jsmn_parser *parser,
    const char *json,
    size_t length,
    jsmntok_t *tokens,
    unsigned int token_capacity
);

#ifndef JSMN_HEADER
static jsmntok_t *jsmn_alloc_token(
    jsmn_parser *parser,
    jsmntok_t *tokens,
    size_t token_capacity
)
{
    jsmntok_t *token;

    if (parser->toknext >= token_capacity) {
        return NULL;
    }
    token = &tokens[parser->toknext++];
    token->start = -1;
    token->end = -1;
    token->size = 0;
    token->parent = -1;
    return token;
}

static void jsmn_fill_token(
    jsmntok_t *token,
    jsmntype_t type,
    int start,
    int end
)
{
    token->type = type;
    token->start = start;
    token->end = end;
    token->size = 0;
}

static int jsmn_parse_primitive(
    jsmn_parser *parser,
    const char *json,
    size_t length,
    jsmntok_t *tokens,
    size_t token_capacity
)
{
    int start = (int)parser->pos;
    jsmntok_t *token;

    for (; parser->pos < length && json[parser->pos] != '\0'; parser->pos++) {
        char character = json[parser->pos];

        if (character == '\t' || character == '\r' || character == '\n' ||
            character == ' ' || character == ',' || character == ']' ||
            character == '}') {
            break;
        }
        if (character < 32 || character >= 127) {
            parser->pos = (unsigned int)start;
            return JSMN_ERROR_INVAL;
        }
    }

    token = jsmn_alloc_token(parser, tokens, token_capacity);
    if (token == NULL) {
        parser->pos = (unsigned int)start;
        return JSMN_ERROR_NOMEM;
    }
    jsmn_fill_token(token, JSMN_PRIMITIVE, start, (int)parser->pos);
    token->parent = parser->toksuper;
    parser->pos--;
    return 0;
}

static int jsmn_parse_string(
    jsmn_parser *parser,
    const char *json,
    size_t length,
    jsmntok_t *tokens,
    size_t token_capacity
)
{
    int start = (int)parser->pos;
    jsmntok_t *token;

    parser->pos++;
    for (; parser->pos < length && json[parser->pos] != '\0'; parser->pos++) {
        char character = json[parser->pos];

        if (character == '"') {
            token = jsmn_alloc_token(parser, tokens, token_capacity);
            if (token == NULL) {
                parser->pos = (unsigned int)start;
                return JSMN_ERROR_NOMEM;
            }
            jsmn_fill_token(token, JSMN_STRING, start + 1, (int)parser->pos);
            token->parent = parser->toksuper;
            return 0;
        }
        if (character == '\\') {
            if (parser->pos + 1U >= length) {
                break;
            }
            parser->pos++;
            character = json[parser->pos];
            if (character == 'u') {
                for (int index = 0; index < 4; index++) {
                    parser->pos++;
                    if (parser->pos >= length ||
                        !((json[parser->pos] >= '0' && json[parser->pos] <= '9') ||
                          (json[parser->pos] >= 'A' && json[parser->pos] <= 'F') ||
                          (json[parser->pos] >= 'a' && json[parser->pos] <= 'f'))) {
                        parser->pos = (unsigned int)start;
                        return JSMN_ERROR_INVAL;
                    }
                }
            } else if (character != '"' && character != '/' &&
                       character != '\\' && character != 'b' &&
                       character != 'f' && character != 'r' &&
                       character != 'n' && character != 't') {
                parser->pos = (unsigned int)start;
                return JSMN_ERROR_INVAL;
            }
        }
    }
    parser->pos = (unsigned int)start;
    return JSMN_ERROR_PART;
}

JSMN_API int jsmn_parse(
    jsmn_parser *parser,
    const char *json,
    size_t length,
    jsmntok_t *tokens,
    unsigned int token_capacity
)
{
    int result;

    for (; parser->pos < length && json[parser->pos] != '\0'; parser->pos++) {
        char character = json[parser->pos];

        switch (character) {
        case '{':
        case '[': {
            jsmntok_t *token = jsmn_alloc_token(parser, tokens, token_capacity);
            if (token == NULL) {
                return JSMN_ERROR_NOMEM;
            }
            if (parser->toksuper != -1) {
                tokens[parser->toksuper].size++;
                token->parent = parser->toksuper;
            }
            token->type = character == '{' ? JSMN_OBJECT : JSMN_ARRAY;
            token->start = (int)parser->pos;
            parser->toksuper = (int)parser->toknext - 1;
            break;
        }
        case '}':
        case ']': {
            jsmntype_t type = character == '}' ? JSMN_OBJECT : JSMN_ARRAY;
            int index;

            for (index = (int)parser->toknext - 1; index >= 0; index--) {
                jsmntok_t *token = &tokens[index];
                if (token->start != -1 && token->end == -1) {
                    if (token->type != type) {
                        return JSMN_ERROR_INVAL;
                    }
                    token->end = (int)parser->pos + 1;
                    parser->toksuper = token->parent;
                    break;
                }
            }
            if (index == -1) {
                return JSMN_ERROR_INVAL;
            }
            break;
        }
        case '"':
            result = jsmn_parse_string(
                parser, json, length, tokens, token_capacity
            );
            if (result < 0) {
                return result;
            }
            if (parser->toksuper != -1) {
                tokens[parser->toksuper].size++;
            }
            break;
        case '\t':
        case '\r':
        case '\n':
        case ' ':
        case ':':
        case ',':
            break;
        default:
            result = jsmn_parse_primitive(
                parser, json, length, tokens, token_capacity
            );
            if (result < 0) {
                return result;
            }
            if (parser->toksuper != -1) {
                tokens[parser->toksuper].size++;
            }
            break;
        }
    }

    for (unsigned int index = 0; index < parser->toknext; index++) {
        if (tokens[index].start != -1 && tokens[index].end == -1) {
            return JSMN_ERROR_PART;
        }
    }
    return (int)parser->toknext;
}

JSMN_API void jsmn_init(jsmn_parser *parser)
{
    parser->pos = 0U;
    parser->toknext = 0U;
    parser->toksuper = -1;
}
#endif

#endif

