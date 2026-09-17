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
 * zunjin 语言 - 语法解析器接口
 * 递归下降解析器
 * 完全独立重新实现
 * ========================================================================== */

#ifndef ZUNJIN_PARSER_H
#define ZUNJIN_PARSER_H

#include "zunjin.h"
#include "lexer.h"
#include "ast.h"

/* ---------- 解析器 ---------- */
struct ZjnParser {
    ZjnLexer* lexer;
    ZjnToken* current;
    ZjnToken* next;
    ZjnError error;
};

/* ---------- 辅助宏 ---------- */
#define CUR_TYPE  (parser->current ? parser->current->type : TOK_EOF)
#define NEXT_TYPE (parser->next ? parser->next->type : TOK_EOF)
#define CUR_LINE  (parser->current ? parser->current->line : 0)
#define CUR_COL   (parser->current ? parser->current->column : 0)

/* ---------- Token 管理 ---------- */
void advance_parser(ZjnParser* parser);
void init_parser(ZjnParser* parser);
void check_lexer_error(ZjnParser* parser);
void expect(ZjnParser* parser, ZjnTokenType type);
void skip_newlines(ZjnParser* parser);

/* ---------- 函数 ---------- */
ZjnParser* zjn_parser_create(ZjnLexer* lexer);
void zjn_parser_free(ZjnParser* parser);
ZjnAstNode* zjn_parser_parse(ZjnParser* parser);

/* 核心解析 */
ZjnAstNode* parse_block(ZjnParser* parser);
ZjnAstNode* parse_statement(ZjnParser* parser);
ZjnAstNode* parse_simple_stmt(ZjnParser* parser);

/* 语句解析 */
ZjnAstNode* parse_if_stmt(ZjnParser* parser);
ZjnAstNode* parse_while_stmt(ZjnParser* parser);
ZjnAstNode* parse_for_stmt(ZjnParser* parser);
ZjnAstNode* parse_func_def(ZjnParser* parser);
ZjnAstNode* parse_return_stmt(ZjnParser* parser);
ZjnAstNode* parse_try_stmt(ZjnParser* parser);
ZjnAstNode* parse_raise_stmt(ZjnParser* parser);
ZjnAstNode* parse_import_stmt(ZjnParser* parser);

/* 表达式解析 */
ZjnAstNode* parse_expression(ZjnParser* parser);

#endif /* ZUNJIN_PARSER_H */