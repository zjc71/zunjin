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
 * zunjin 语言 - 语法解析器实现（核心）
 * 递归下降解析
 * 完全独立重新实现，将 Token 流构建为 AST
 *
 * 版本 2.0 新增语法：
 *  - 复合赋值：a += 1 / a **= 2 等
 *  - 索引赋值：lst[0] = v / d["k"] = v
 *  - 幂运算 **（右结合，-2**2 = -4）与位运算 << >> & | ^ ~
 *  - 成员测试：x of list / x negate of list（not in）
 *  - 字典字面量 {key: value, ...}
 *  - 切片：a[1:3] / a[1:3:2] / a[:] / a[::-1]
 *  - 默认参数：proc f(a, b=10)
 *  - 异常处理：attempt/seize/settle 与 fling
 *  - TOK_ERROR 显式报错
 * ========================================================================== */

#include "parser.h"
#include <string.h>
#include <stdio.h>

/* ===================================================================
 *  解析器创建/销毁
 * =================================================================== */

ZjnParser* zjn_parser_create(ZjnLexer* lexer) {
    ZjnParser* parser = ZUNJIN_ALLOC(ZjnParser);
    if (!parser) return NULL;
    parser->lexer = lexer;
    parser->current = NULL;
    parser->next = NULL;
    memset(&parser->error, 0, sizeof(ZjnError));
    return parser;
}

void zjn_parser_free(ZjnParser* parser) {
    if (!parser) return;
    if (parser->current) zjn_token_free(parser->current);
    if (parser->next) zjn_token_free(parser->next);
    free(parser);
}

/* ---------- Token 管理 ---------- */
void advance_parser(ZjnParser* parser) {
    if (parser->current) {
        zjn_token_free(parser->current);
    }
    parser->current = parser->next;
    parser->next = zjn_lexer_next(parser->lexer);
}

void init_parser(ZjnParser* parser) {
    parser->current = NULL;
    parser->next = zjn_lexer_next(parser->lexer);
    advance_parser(parser); /* 加载第一个 Token */
}

/* 若当前 Token 是词法错误，抛出其携带的消息 */
void check_lexer_error(ZjnParser* parser) {
    if (CUR_TYPE == TOK_ERROR && parser->current) {
        ZJN_THROW(parser->current->data.ident_val ? parser->current->data.ident_val
                                                  : "词法错误",
                  CUR_LINE, CUR_COL);
    }
}

void expect(ZjnParser* parser, ZjnTokenType type) {
    check_lexer_error(parser);
    if (CUR_TYPE != type) {
        char msg[256];
        snprintf(msg, sizeof(msg), "期望 %s, 实际得到 %s",
                 zjn_token_type_name(type),
                 zjn_token_type_name(CUR_TYPE));
        ZJN_THROW(msg, parser->current ? parser->current->line : 0,
                  parser->current ? parser->current->column : 0);
    }
    advance_parser(parser);
}

void skip_newlines(ZjnParser* parser) {
    while (CUR_TYPE == TOK_NEWLINE) {
        advance_parser(parser);
    }
}

/* ===================================================================
 *  代码块解析：解析 INDENT -> 语句列表 -> DEDENT
 *  前提：INDENT 已被消费
 *  结果：DEDENT 被消费，返回 BLOCK 节点
 * =================================================================== */
ZjnAstNode* parse_block(ZjnParser* parser) {
    ZjnAstNode** stmts = NULL;
    int count = 0, cap = 0;

    while (CUR_TYPE != TOK_DEDENT && CUR_TYPE != TOK_EOF) {
        if (CUR_TYPE == TOK_NEWLINE) {
            advance_parser(parser);
            continue;
        }
        if (count >= cap) {
            cap = cap == 0 ? 16 : cap * 2;
            stmts = realloc(stmts, cap * sizeof(ZjnAstNode*));
        }
        stmts[count++] = parse_statement(parser);
    }

    /* 消费 DEDENT */
    if (CUR_TYPE == TOK_DEDENT) {
        advance_parser(parser);
    }
    skip_newlines(parser);

    return zjn_ast_block(stmts, count);
}

/* ===================================================================
 *  语句解析分发
 * =================================================================== */
ZjnAstNode* parse_statement(ZjnParser* parser) {
    check_lexer_error(parser);
    switch (CUR_TYPE) {
        case TOK_IF:       return parse_if_stmt(parser);
        case TOK_WHILE:    return parse_while_stmt(parser);
        case TOK_FOR:      return parse_for_stmt(parser);
        case TOK_FUNC:     return parse_func_def(parser);
        case TOK_RETURN:   return parse_return_stmt(parser);
        case TOK_IMPORT:   return parse_import_stmt(parser);
        case TOK_TRY:      return parse_try_stmt(parser);
        case TOK_RAISE:    return parse_raise_stmt(parser);
        case TOK_PASS: {
            int line = CUR_LINE, col = CUR_COL;
            advance_parser(parser);
            return zjn_ast_pass_stmt(line, col);
        }
        case TOK_BREAK: {
            int line = CUR_LINE, col = CUR_COL;
            advance_parser(parser);
            return zjn_ast_break_stmt(line, col);
        }
        case TOK_CONTINUE: {
            int line = CUR_LINE, col = CUR_COL;
            advance_parser(parser);
            return zjn_ast_continue_stmt(line, col);
        }
        default:
            return parse_simple_stmt(parser);
    }
}

/* ===================================================================
 *  简单语句（表达式/赋值/复合赋值/索引赋值）
 * =================================================================== */
ZjnAstNode* parse_simple_stmt(ZjnParser* parser) {
    ZjnAstNode* expr = parse_expression(parser);

    /* 复合赋值：name += expr 等 */
    if (CUR_TYPE >= TOK_PLUS_ASSIGN && CUR_TYPE <= TOK_POW_ASSIGN) {
        if (expr->type != ZNODE_VARIABLE) {
            ZJN_THROW("复合赋值左侧必须是变量名",
                      expr->line, expr->column);
        }
        static const ZjnOp op_map[] = {
            ZOP_ADD, ZOP_SUB, ZOP_MUL, ZOP_DIV,
            ZOP_FLOORDIV, ZOP_MOD, ZOP_POW
        };
        int op_idx = CUR_TYPE - TOK_PLUS_ASSIGN;
        ZjnOp op = (op_idx >= 0 && op_idx < 7) ? op_map[op_idx] : ZOP_ADD;
        char* var_name = strdup(expr->data.variable.name);
        int line = expr->line, col = expr->column;
        zjn_ast_free(expr);
        advance_parser(parser); /* 消费复合赋值运算符 */
        ZjnAstNode* value = parse_expression(parser);
        ZjnAstNode* result = zjn_ast_op_assign(var_name, op, value, line, col);
        free(var_name);
        return result;
    }

    if (CUR_TYPE == TOK_ASSIGN) {
        /* 变量赋值 */
        if (expr->type == ZNODE_VARIABLE) {
            char* var_name = strdup(expr->data.variable.name);
            int line = expr->line, col = expr->column;
            zjn_ast_free(expr);
            advance_parser(parser); /* 消费 = */
            ZjnAstNode* value = parse_expression(parser);
            ZjnAstNode* result = zjn_ast_assign(var_name, value, line, col);
            free(var_name);
            return result;
        }
        /* 索引赋值：obj[index] = value */
        if (expr->type == ZNODE_INDEX_EXPR) {
            int line = expr->line, col = expr->column;
            ZjnAstNode* obj = expr->data.index_expr.obj;
            ZjnAstNode* index = expr->data.index_expr.index;
            free(expr); /* 仅释放节点外壳，子节点已取出 */
            advance_parser(parser); /* 消费 = */
            ZjnAstNode* value = parse_expression(parser);
            return zjn_ast_index_assign(obj, index, value, line, col);
        }
        ZJN_THROW("赋值左侧必须是变量名或索引表达式",
                  expr->line, expr->column);
    }

    return zjn_ast_expr_stmt(expr, expr->line, expr->column);
}

/* ===================================================================
 *  主入口
 * =================================================================== */
ZjnAstNode* zjn_parser_parse(ZjnParser* parser) {
    init_parser(parser);

    ZjnAstNode** stmts = NULL;
    int count = 0, cap = 0;

    while (CUR_TYPE != TOK_EOF) {
        if (CUR_TYPE == TOK_NEWLINE) {
            advance_parser(parser);
            continue;
        }
        if (count >= cap) {
            cap = cap == 0 ? 16 : cap * 2;
            stmts = realloc(stmts, cap * sizeof(ZjnAstNode*));
        }
        stmts[count++] = parse_statement(parser);
    }

    return zjn_ast_program(stmts, count);
}