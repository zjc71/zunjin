/*
 * zunjin - A script programming language
 *
 * This file is part of the zunjin project.
 *
 * zunjin is dual-licensed:
 * 1. LGPL v3 (GNU Lesser General Public License v3)
 * 2. Commercial License (copyright assigned to project owner)
 *
 * For full license terms, see LICENSE and CLA.md files.
 *
 * SPDX-License-Identifier: LGPL-3.0-only OR Commercial
 */

/* ==========================================================================
 * zunjin 语言 - 词法分析器接口
 * 完全独立重新实现
 *
 * 版本 2.0 新增：
 *  - 运算符：** << >> & | ^ ~（幂与位运算）
 *  - 复合赋值：+= -= *= /= //= %= **=
 *  - 大括号 {}（字典字面量）
 *  - 异常关键字：attempt / seize / settle / fling
 * ========================================================================== */

#ifndef ZUNJIN_LEXER_H
#define ZUNJIN_LEXER_H

#include "zunjin.h"

/* ---------- Token 类型 ---------- */
typedef enum {
    /* 字面量 */
    TOK_NUMBER,
    TOK_STRING,
    TOK_IDENTIFIER,

    /* 关键字 */
    TOK_IF, TOK_ELIF, TOK_ELSE,
    TOK_WHILE, TOK_FOR, TOK_IN,
    TOK_FUNC, TOK_RETURN,
    TOK_AND, TOK_OR, TOK_NOT,
    TOK_TRUE, TOK_FALSE, TOK_NONE,
    TOK_BREAK, TOK_CONTINUE,
    TOK_IMPORT, TOK_PASS,

    /* 异常处理关键字 */
    TOK_TRY, TOK_CATCH, TOK_FINALLY, TOK_RAISE,

    /* 算术运算符 */
    TOK_PLUS, TOK_MINUS, TOK_STAR, TOK_SLASH,
    TOK_DBL_SLASH, TOK_PERCENT, TOK_POW,

    /* 位运算符 */
    TOK_SHL, TOK_SHR,
    TOK_BAND, TOK_BOR, TOK_BXOR, TOK_BNOT,

    /* 赋值 */
    TOK_ASSIGN,
    TOK_PLUS_ASSIGN, TOK_MINUS_ASSIGN, TOK_STAR_ASSIGN,
    TOK_SLASH_ASSIGN, TOK_DBL_SLASH_ASSIGN, TOK_PERCENT_ASSIGN,
    TOK_POW_ASSIGN,

    /* 比较 */
    TOK_EQ, TOK_NEQ, TOK_LT, TOK_GT, TOK_LE, TOK_GE,
    TOK_NOT_IN,

    /* 分隔符 */
    TOK_LPAREN, TOK_RPAREN,
    TOK_LBRACKET, TOK_RBRACKET,
    TOK_LBRACE, TOK_RBRACE,
    TOK_COMMA, TOK_COLON, TOK_DOT,

    /* 缩进与结构 */
    TOK_NEWLINE, TOK_INDENT, TOK_DEDENT,

    /* 特殊 */
    TOK_EOF, TOK_ERROR,
} ZjnTokenType;

/* ---------- Token ---------- */
struct ZjnToken {
    ZjnTokenType type;
    union {
        long     int_val;
        double   float_val;
        char*    string_val;
        char*    ident_val;
    } data;
    int is_float;
    int line;
    int column;
};

/* ---------- 词法分析器 ---------- */
struct ZjnLexer {
    const char* source;
    int source_len;
    int pos;
    int line;
    int col;

    /* 缩进栈 */
    int* indent_stack;
    int indent_top;
    int indent_capacity;

    /* 当前行缩进 */
    int current_indent;
    int pending_dedents;

    /* 括号嵌套深度（>0 时换行不产生 NEWLINE/INDENT/DEDENT） */
    int bracket_depth;

    /* 错误 */
    ZjnError error;
};

/* ---------- 函数 ---------- */
ZjnLexer* zjn_lexer_create(const char* source);
void zjn_lexer_free(ZjnLexer* lexer);
ZjnToken* zjn_lexer_next(ZjnLexer* lexer);
void zjn_token_free(ZjnToken* token);
const char* zjn_token_type_name(ZjnTokenType type);

#endif /* ZUNJIN_LEXER_H */
