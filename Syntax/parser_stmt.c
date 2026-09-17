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
 * zunjin 语言 - 语法解析器实现（语句解析）
 * 递归下降解析 - 语句级别的解析逻辑
 * 完全独立重新实现
 *
 * 包含: if/elif/else, while, for, func, return, try/except/finally,
 *       raise, import
 * ========================================================================== */

#include "parser.h"
#include <string.h>
#include <stdio.h>

/* ===================================================================
 *  if 语句
 * =================================================================== */
ZjnAstNode* parse_if_stmt(ZjnParser* parser) {
    int line = CUR_LINE, col = CUR_COL;
    advance_parser(parser); /* 消费 if */
    ZjnAstNode* cond = parse_expression(parser);
    expect(parser, TOK_COLON);
    expect(parser, TOK_NEWLINE);
    expect(parser, TOK_INDENT);
    ZjnAstNode* then_body = parse_block(parser);

    /* 解析 whenelse 分支 */
    int elif_count = 0;
    ZjnElifPair* elif_pairs = NULL;

    while (CUR_TYPE == TOK_ELIF) {
        advance_parser(parser); /* 消费 elif */
        ZjnAstNode* elif_cond = parse_expression(parser);
        expect(parser, TOK_COLON);
        expect(parser, TOK_NEWLINE);
        expect(parser, TOK_INDENT);
        ZjnAstNode* elif_body = parse_block(parser);

        elif_pairs = realloc(elif_pairs, (elif_count + 1) * sizeof(ZjnElifPair));
        elif_pairs[elif_count].condition = elif_cond;
        elif_pairs[elif_count].body = elif_body;
        elif_count++;
    }

    /* 解析 else 分支 */
    ZjnAstNode* else_body = NULL;
    if (CUR_TYPE == TOK_ELSE) {
        advance_parser(parser); /* 消费 else */
        expect(parser, TOK_COLON);
        expect(parser, TOK_NEWLINE);
        expect(parser, TOK_INDENT);
        else_body = parse_block(parser);
    }

    return zjn_ast_if_stmt(cond, then_body, elif_pairs, elif_count, else_body, line, col);
}

/* ===================================================================
 *  while 循环
 * =================================================================== */
ZjnAstNode* parse_while_stmt(ZjnParser* parser) {
    int line = CUR_LINE, col = CUR_COL;
    advance_parser(parser); /* 消费 while */
    ZjnAstNode* cond = parse_expression(parser);
    expect(parser, TOK_COLON);
    expect(parser, TOK_NEWLINE);
    expect(parser, TOK_INDENT);
    ZjnAstNode* body = parse_block(parser);
    return zjn_ast_while_stmt(cond, body, line, col);
}

/* ===================================================================
 *  for 循环
 * =================================================================== */
ZjnAstNode* parse_for_stmt(ZjnParser* parser) {
    int line = CUR_LINE, col = CUR_COL;
    advance_parser(parser); /* 消费 for */

    /* 解析循环变量名 */
    if (CUR_TYPE != TOK_IDENTIFIER) {
        ZJN_THROW("for 循环需要变量名",
                  parser->current ? parser->current->line : 0,
                  parser->current ? parser->current->column : 0);
    }
    /* 必须立即 strdup，因为 advance_parser 会释放当前 token */
    char* var_name = strdup(parser->current->data.ident_val);
    advance_parser(parser); /* 消费变量名 */

    expect(parser, TOK_IN); /* 消费 in */

    ZjnAstNode* iterable = parse_expression(parser);
    expect(parser, TOK_COLON);
    expect(parser, TOK_NEWLINE);
    expect(parser, TOK_INDENT);
    ZjnAstNode* body = parse_block(parser);

    return zjn_ast_for_stmt(var_name, iterable, body, line, col);
}

/* ===================================================================
 *  函数定义（支持默认参数，默认参数必须位于参数列表末尾）
 * =================================================================== */
ZjnAstNode* parse_func_def(ZjnParser* parser) {
    int line = CUR_LINE, col = CUR_COL;
    advance_parser(parser); /* 消费 func */

    /* 解析函数名 */
    if (CUR_TYPE != TOK_IDENTIFIER) {
        ZJN_THROW("函数定义需要函数名",
                  parser->current ? parser->current->line : 0,
                  parser->current ? parser->current->column : 0);
    }
    /* 必须立即 strdup，因为 advance_parser 会释放当前 token */
    char* func_name = strdup(parser->current->data.ident_val);
    advance_parser(parser); /* 消费函数名 */

    /* 解析参数列表 */
    expect(parser, TOK_LPAREN);
    skip_newlines(parser); /* 括号内允许换行 */
    char** params = NULL;
    ZjnAstNode** defaults = NULL;
    int param_count = 0, param_cap = 0;
    int seen_default = 0;

    if (CUR_TYPE != TOK_RPAREN) {
        /* 解析第一个参数 */
        if (CUR_TYPE != TOK_IDENTIFIER) {
            ZJN_THROW("参数名必须是标识符",
                      parser->current->line, parser->current->column);
        }
        param_cap = 4;
        params = malloc(param_cap * sizeof(char*));
        defaults = calloc(param_cap, sizeof(ZjnAstNode*));
        params[param_count] = strdup(parser->current->data.ident_val);
        advance_parser(parser);

        /* 默认值 */
        if (CUR_TYPE == TOK_ASSIGN) {
            advance_parser(parser);
            defaults[param_count] = parse_expression(parser);
            seen_default = 1;
        }
        param_count++;

        /* 解析后续参数 */
        while (CUR_TYPE == TOK_COMMA) {
            advance_parser(parser); /* 消费逗号 */
            skip_newlines(parser); /* 括号内允许换行 */
            if (CUR_TYPE == TOK_RPAREN) break; /* 允许尾随逗号 */
            if (CUR_TYPE != TOK_IDENTIFIER) {
                ZJN_THROW("参数名必须是标识符",
                          parser->current->line, parser->current->column);
            }
            if (param_count >= param_cap) {
                param_cap *= 2;
                params = realloc(params, param_cap * sizeof(char*));
                defaults = realloc(defaults, param_cap * sizeof(ZjnAstNode*));
                for (int i = param_count; i < param_cap; i++) defaults[i] = NULL;
            }
            params[param_count] = strdup(parser->current->data.ident_val);
            advance_parser(parser);
            if (CUR_TYPE == TOK_ASSIGN) {
                advance_parser(parser);
                defaults[param_count] = parse_expression(parser);
                seen_default = 1;
            } else if (seen_default) {
                ZJN_THROW("无默认值的参数不能位于有默认值的参数之后",
                          parser->current->line, parser->current->column);
            }
            param_count++;
        }
    }
    skip_newlines(parser);
    expect(parser, TOK_RPAREN);

    /* 解析函数体 */
    expect(parser, TOK_COLON);
    expect(parser, TOK_NEWLINE);
    expect(parser, TOK_INDENT);
    ZjnAstNode* body = parse_block(parser);

    ZjnAstNode* result = zjn_ast_func_def(func_name, params, param_count,
                                          defaults, body, line, col);
    free(func_name);
    return result;
}

/* ===================================================================
 *  return 语句
 * =================================================================== */
ZjnAstNode* parse_return_stmt(ZjnParser* parser) {
    int line = CUR_LINE, col = CUR_COL;
    advance_parser(parser); /* 消费 return */

    /* 如果下一个 token 是 NEWLINE 或 DEDENT，则 return 无返回值 */
    if (CUR_TYPE == TOK_NEWLINE || CUR_TYPE == TOK_DEDENT ||
        CUR_TYPE == TOK_EOF) {
        return zjn_ast_return_stmt(NULL, line, col);
    }

    /* 否则解析返回值表达式 */
    ZjnAstNode* value = parse_expression(parser);
    return zjn_ast_return_stmt(value, line, col);
}

/* ===================================================================
 *  异常处理：attempt/seize/settle 与 fling
 * =================================================================== */
ZjnAstNode* parse_try_stmt(ZjnParser* parser) {
    int line = CUR_LINE, col = CUR_COL;
    advance_parser(parser); /* 消费 attempt */

    expect(parser, TOK_COLON);
    expect(parser, TOK_NEWLINE);
    expect(parser, TOK_INDENT);
    ZjnAstNode* try_body = parse_block(parser);

    /* seize [var]: body */
    ZjnAstNode* catch_body = NULL;
    char* catch_var = NULL;
    if (CUR_TYPE == TOK_CATCH) {
        advance_parser(parser);
        if (CUR_TYPE == TOK_IDENTIFIER) {
            catch_var = strdup(parser->current->data.ident_val);
            advance_parser(parser);
        }
        expect(parser, TOK_COLON);
        expect(parser, TOK_NEWLINE);
        expect(parser, TOK_INDENT);
        catch_body = parse_block(parser);
    }

    /* settle: body */
    ZjnAstNode* finally_body = NULL;
    if (CUR_TYPE == TOK_FINALLY) {
        advance_parser(parser);
        expect(parser, TOK_COLON);
        expect(parser, TOK_NEWLINE);
        expect(parser, TOK_INDENT);
        finally_body = parse_block(parser);
    }

    ZjnAstNode* result = zjn_ast_try_stmt(try_body, catch_var,
                                          catch_body, finally_body,
                                          line, col);
    if (catch_var) free(catch_var);
    return result;
}

ZjnAstNode* parse_raise_stmt(ZjnParser* parser) {
    int line = CUR_LINE, col = CUR_COL;
    advance_parser(parser); /* 消费 fling */

    if (CUR_TYPE == TOK_NEWLINE || CUR_TYPE == TOK_DEDENT ||
        CUR_TYPE == TOK_EOF) {
        return zjn_ast_raise_stmt(NULL, line, col);
    }
    ZjnAstNode* value = parse_expression(parser);
    return zjn_ast_raise_stmt(value, line, col);
}

/* ===================================================================
 *  import 语句
 * =================================================================== */
ZjnAstNode* parse_import_stmt(ZjnParser* parser) {
    int line = CUR_LINE, col = CUR_COL;
    advance_parser(parser); /* 消费 import */

    if (CUR_TYPE != TOK_IDENTIFIER) {
        ZJN_THROW("import 需要模块名",
                  parser->current ? parser->current->line : 0,
                  parser->current ? parser->current->column : 0);
    }
    char* module_name = strdup(parser->current->data.ident_val);
    advance_parser(parser); /* 消费模块名 */

    ZjnAstNode* result = zjn_ast_import_stmt(module_name, line, col);
    free(module_name);
    return result;
}