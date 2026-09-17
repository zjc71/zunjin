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
 * zunjin 语言 - 语法解析器实现（表达式解析）
 * 递归下降解析 - 表达式级别的解析逻辑
 * 完全独立重新实现
 *
 * 表达式优先级（从低到高）:
 *  or -> and -> not -> 比较 -> 位或 -> 位异或 -> 位与 -> 移位
 *  -> 加减 -> 乘除 -> 一元 -> 幂 -> 后缀 -> 原子
 * ========================================================================== */

#include "parser.h"
#include <string.h>
#include <stdio.h>

/* ---------- 前向声明 ---------- */
static ZjnAstNode* parse_or(ZjnParser* parser);
static ZjnAstNode* parse_and(ZjnParser* parser);
static ZjnAstNode* parse_not(ZjnParser* parser);
static ZjnAstNode* parse_comparison(ZjnParser* parser);
static ZjnAstNode* parse_bit_or(ZjnParser* parser);
static ZjnAstNode* parse_bit_xor(ZjnParser* parser);
static ZjnAstNode* parse_bit_and(ZjnParser* parser);
static ZjnAstNode* parse_shift(ZjnParser* parser);
static ZjnAstNode* parse_term(ZjnParser* parser);
static ZjnAstNode* parse_factor(ZjnParser* parser);
static ZjnAstNode* parse_unary(ZjnParser* parser);
static ZjnAstNode* parse_power(ZjnParser* parser);
static ZjnAstNode* parse_postfix(ZjnParser* parser);
static ZjnAstNode* parse_primary(ZjnParser* parser);

/* ===================================================================
 *  表达式解析入口
 * =================================================================== */

ZjnAstNode* parse_expression(ZjnParser* parser) {
    return parse_or(parser);
}

static ZjnAstNode* parse_or(ZjnParser* parser) {
    ZjnAstNode* left = parse_and(parser);
    while (CUR_TYPE == TOK_OR) {
        int line = CUR_LINE, col = CUR_COL;
        advance_parser(parser);
        ZjnAstNode* right = parse_and(parser);
        left = zjn_ast_binary_op(ZOP_OR, left, right, line, col);
    }
    return left;
}

static ZjnAstNode* parse_and(ZjnParser* parser) {
    ZjnAstNode* left = parse_not(parser);
    while (CUR_TYPE == TOK_AND) {
        int line = CUR_LINE, col = CUR_COL;
        advance_parser(parser);
        ZjnAstNode* right = parse_not(parser);
        left = zjn_ast_binary_op(ZOP_AND, left, right, line, col);
    }
    return left;
}

static ZjnAstNode* parse_not(ZjnParser* parser) {
    if (CUR_TYPE == TOK_NOT) {
        int line = CUR_LINE, col = CUR_COL;
        advance_parser(parser);
        ZjnAstNode* operand = parse_not(parser);
        return zjn_ast_unary_op(ZOP_NOT, operand, line, col);
    }
    return parse_comparison(parser);
}

static ZjnAstNode* parse_comparison(ZjnParser* parser) {
    ZjnAstNode* left = parse_bit_or(parser);
    while (CUR_TYPE == TOK_EQ || CUR_TYPE == TOK_NEQ ||
           CUR_TYPE == TOK_LT || CUR_TYPE == TOK_GT ||
           CUR_TYPE == TOK_LE || CUR_TYPE == TOK_GE ||
           CUR_TYPE == TOK_IN ||
           /* x negate of y */
           (CUR_TYPE == TOK_NOT && NEXT_TYPE == TOK_IN)) {
        ZjnOp op = ZOP_NONE;
        int line = CUR_LINE, col = CUR_COL;
        if (CUR_TYPE == TOK_IN) {
            op = ZOP_IN;
            advance_parser(parser);
        } else if (CUR_TYPE == TOK_NOT && NEXT_TYPE == TOK_IN) {
            op = ZOP_NOT_IN;
            advance_parser(parser);
            advance_parser(parser);
        } else {
            switch (CUR_TYPE) {
                case TOK_EQ:  op = ZOP_EQ;  break;
                case TOK_NEQ: op = ZOP_NEQ; break;
                case TOK_LT:  op = ZOP_LT;  break;
                case TOK_GT:  op = ZOP_GT;  break;
                case TOK_LE:  op = ZOP_LE;  break;
                case TOK_GE:  op = ZOP_GE;  break;
                default: break;
            }
            advance_parser(parser);
        }
        ZjnAstNode* right = parse_bit_or(parser);
        left = zjn_ast_binary_op(op, left, right, line, col);
    }
    return left;
}

static ZjnAstNode* parse_bit_or(ZjnParser* parser) {
    ZjnAstNode* left = parse_bit_xor(parser);
    while (CUR_TYPE == TOK_BOR) {
        int line = CUR_LINE, col = CUR_COL;
        advance_parser(parser);
        ZjnAstNode* right = parse_bit_xor(parser);
        left = zjn_ast_binary_op(ZOP_BOR, left, right, line, col);
    }
    return left;
}

static ZjnAstNode* parse_bit_xor(ZjnParser* parser) {
    ZjnAstNode* left = parse_bit_and(parser);
    while (CUR_TYPE == TOK_BXOR) {
        int line = CUR_LINE, col = CUR_COL;
        advance_parser(parser);
        ZjnAstNode* right = parse_bit_and(parser);
        left = zjn_ast_binary_op(ZOP_BXOR, left, right, line, col);
    }
    return left;
}

static ZjnAstNode* parse_bit_and(ZjnParser* parser) {
    ZjnAstNode* left = parse_shift(parser);
    while (CUR_TYPE == TOK_BAND) {
        int line = CUR_LINE, col = CUR_COL;
        advance_parser(parser);
        ZjnAstNode* right = parse_shift(parser);
        left = zjn_ast_binary_op(ZOP_BAND, left, right, line, col);
    }
    return left;
}

static ZjnAstNode* parse_shift(ZjnParser* parser) {
    ZjnAstNode* left = parse_term(parser);
    while (CUR_TYPE == TOK_SHL || CUR_TYPE == TOK_SHR) {
        ZjnOp op = CUR_TYPE == TOK_SHL ? ZOP_SHL : ZOP_SHR;
        int line = CUR_LINE, col = CUR_COL;
        advance_parser(parser);
        ZjnAstNode* right = parse_term(parser);
        left = zjn_ast_binary_op(op, left, right, line, col);
    }
    return left;
}

static ZjnAstNode* parse_term(ZjnParser* parser) {
    ZjnAstNode* left = parse_factor(parser);
    while (CUR_TYPE == TOK_PLUS || CUR_TYPE == TOK_MINUS) {
        ZjnOp op = CUR_TYPE == TOK_PLUS ? ZOP_ADD : ZOP_SUB;
        int line = CUR_LINE, col = CUR_COL;
        advance_parser(parser);
        ZjnAstNode* right = parse_factor(parser);
        left = zjn_ast_binary_op(op, left, right, line, col);
    }
    return left;
}

static ZjnAstNode* parse_factor(ZjnParser* parser) {
    ZjnAstNode* left = parse_unary(parser);
    while (CUR_TYPE == TOK_STAR || CUR_TYPE == TOK_SLASH ||
           CUR_TYPE == TOK_DBL_SLASH || CUR_TYPE == TOK_PERCENT) {
        ZjnOp op = ZOP_NONE;
        int line = CUR_LINE, col = CUR_COL;
        switch (CUR_TYPE) {
            case TOK_STAR:      op = ZOP_MUL;       break;
            case TOK_SLASH:     op = ZOP_DIV;       break;
            case TOK_DBL_SLASH: op = ZOP_FLOORDIV;  break;
            case TOK_PERCENT:   op = ZOP_MOD;       break;
            default: break;
        }
        advance_parser(parser);
        ZjnAstNode* right = parse_unary(parser);
        left = zjn_ast_binary_op(op, left, right, line, col);
    }
    return left;
}

/* 一元运算：-x、~x；-2**2 解析为 -(2**2)（与 zunjin 语义一致） */
static ZjnAstNode* parse_unary(ZjnParser* parser) {
    if (CUR_TYPE == TOK_MINUS || CUR_TYPE == TOK_BNOT) {
        ZjnOp op = CUR_TYPE == TOK_MINUS ? ZOP_NEG : ZOP_BNOT;
        int line = CUR_LINE, col = CUR_COL;
        advance_parser(parser);
        ZjnAstNode* operand = parse_unary(parser);
        return zjn_ast_unary_op(op, operand, line, col);
    }
    return parse_power(parser);
}

/* 幂运算：右结合，优先级高于一元运算 */
static ZjnAstNode* parse_power(ZjnParser* parser) {
    ZjnAstNode* left = parse_postfix(parser);
    if (CUR_TYPE == TOK_POW) {
        int line = CUR_LINE, col = CUR_COL;
        advance_parser(parser);
        ZjnAstNode* right = parse_unary(parser); /* 右结合 */
        left = zjn_ast_binary_op(ZOP_POW, left, right, line, col);
    }
    return left;
}

/* ---------- 解析函数调用、索引、切片与属性访问 ---------- */
static ZjnAstNode* parse_postfix(ZjnParser* parser) {
    ZjnAstNode* expr = parse_primary(parser);

    while (CUR_TYPE == TOK_LPAREN || CUR_TYPE == TOK_LBRACKET ||
           CUR_TYPE == TOK_DOT) {
        if (CUR_TYPE == TOK_DOT) {
            /* 属性访问：expr.attr（模块成员） */
            int line = CUR_LINE, col = CUR_COL;
            advance_parser(parser); /* 消费 . */
            if (CUR_TYPE != TOK_IDENTIFIER) {
                ZJN_THROW("属性访问需要标识符",
                          CUR_LINE, CUR_COL);
            }
            char* attr = strdup(parser->current->data.ident_val);
            advance_parser(parser);
            expr = zjn_ast_attr_expr(expr, attr, line, col);
            free(attr);
            continue;
        }
        if (CUR_TYPE == TOK_LPAREN) {
            /* Function call: expr(args) */
            int line = CUR_LINE, col = CUR_COL;
            advance_parser(parser);
            skip_newlines(parser); /* 括号内允许换行 */
            ZjnAstNode** args = NULL;
            int arg_count = 0, arg_cap = 0;

            if (CUR_TYPE != TOK_RPAREN) {
                arg_cap = 4;
                args = malloc(arg_cap * sizeof(ZjnAstNode*));
                args[arg_count++] = parse_expression(parser);

                while (CUR_TYPE == TOK_COMMA) {
                    advance_parser(parser);
                    skip_newlines(parser); /* 括号内允许换行 */
                    if (CUR_TYPE == TOK_RPAREN) break; /* 允许尾随逗号 */
                    if (arg_count >= arg_cap) {
                        arg_cap *= 2;
                        args = realloc(args, arg_cap * sizeof(ZjnAstNode*));
                    }
                    args[arg_count++] = parse_expression(parser);
                }
            }
            skip_newlines(parser);
            expect(parser, TOK_RPAREN);
            expr = zjn_ast_call_expr(expr, args, arg_count, line, col);
        } else {
            /* Index or slice: expr[...] */
            int line = CUR_LINE, col = CUR_COL;
            advance_parser(parser); /* 消费 [ */
            skip_newlines(parser); /* 括号内允许换行 */

            ZjnAstNode* idx = NULL;
            if (CUR_TYPE != TOK_COLON && CUR_TYPE != TOK_RBRACKET) {
                idx = parse_expression(parser);
            }

            if (CUR_TYPE == TOK_COLON) {
                /* 切片：a[start:stop:step] */
                advance_parser(parser); /* 消费第一个 : */
                skip_newlines(parser);
                ZjnAstNode* start = idx;
                ZjnAstNode* stop = NULL;
                if (CUR_TYPE != TOK_COLON && CUR_TYPE != TOK_RBRACKET) {
                    stop = parse_expression(parser);
                }
                ZjnAstNode* step = NULL;
                if (CUR_TYPE == TOK_COLON) {
                    advance_parser(parser);
                    skip_newlines(parser);
                    if (CUR_TYPE != TOK_RBRACKET) {
                        step = parse_expression(parser);
                    }
                }
                skip_newlines(parser);
                expect(parser, TOK_RBRACKET);
                expr = zjn_ast_slice_expr(expr, start, stop, step, line, col);
            } else {
                /* 普通索引 */
                skip_newlines(parser);
                expect(parser, TOK_RBRACKET);
                expr = zjn_ast_index_expr(expr, idx, line, col);
            }
        }
    }

    return expr;
}

static ZjnAstNode* parse_primary(ZjnParser* parser) {
    check_lexer_error(parser);
    switch (CUR_TYPE) {
        case TOK_NUMBER: {
            ZjnToken* t = parser->current;
            int line = t->line, col = t->column;
            ZjnAstNode* node = zjn_ast_number_lit(
                t->data.int_val, t->data.float_val, t->is_float, line, col);
            advance_parser(parser);
            return node;
        }
        case TOK_STRING: {
            int line = CUR_LINE, col = CUR_COL;
            ZjnAstNode* node = zjn_ast_string_lit(
                parser->current->data.string_val, line, col);
            advance_parser(parser);
            return node;
        }
        case TOK_TRUE: {
            int line = CUR_LINE, col = CUR_COL;
            advance_parser(parser);
            return zjn_ast_bool_lit(1, line, col);
        }
        case TOK_FALSE: {
            int line = CUR_LINE, col = CUR_COL;
            advance_parser(parser);
            return zjn_ast_bool_lit(0, line, col);
        }
        case TOK_NONE: {
            int line = CUR_LINE, col = CUR_COL;
            advance_parser(parser);
            return zjn_ast_none_lit(line, col);
        }
        case TOK_IDENTIFIER: {
            int line = CUR_LINE, col = CUR_COL;
            ZjnAstNode* node = zjn_ast_variable(
                parser->current->data.ident_val, line, col);
            advance_parser(parser);
            return node;
        }
        case TOK_LPAREN: {
            advance_parser(parser);
            skip_newlines(parser); /* 括号内允许换行 */
            ZjnAstNode* expr = parse_expression(parser);
            skip_newlines(parser);
            expect(parser, TOK_RPAREN);
            return expr;
        }
        case TOK_LBRACKET: {
            int line = CUR_LINE, col = CUR_COL;
            advance_parser(parser); /* 消费 [ */
            skip_newlines(parser); /* 括号内允许换行 */
            ZjnAstNode** items = NULL;
            int item_count = 0, item_cap = 0;

            if (CUR_TYPE != TOK_RBRACKET) {
                item_cap = 4;
                items = malloc(item_cap * sizeof(ZjnAstNode*));
                items[item_count++] = parse_expression(parser);

                while (CUR_TYPE == TOK_COMMA) {
                    advance_parser(parser); /* 消费逗号 */
                    skip_newlines(parser); /* 括号内允许换行 */
                    if (CUR_TYPE == TOK_RBRACKET) break; /* 允许尾随逗号 */
                    if (item_count >= item_cap) {
                        item_cap *= 2;
                        items = realloc(items, item_cap * sizeof(ZjnAstNode*));
                    }
                    items[item_count++] = parse_expression(parser);
                }
            }
            skip_newlines(parser);
            expect(parser, TOK_RBRACKET);
            return zjn_ast_list_lit(items, item_count, line, col);
        }
        case TOK_LBRACE: {
            /* 字典字面量：{key: value, ...} */
            int line = CUR_LINE, col = CUR_COL;
            advance_parser(parser); /* 消费 { */
            skip_newlines(parser); /* 括号内允许换行 */
            char** keys = NULL;
            ZjnAstNode** values = NULL;
            int item_count = 0, item_cap = 0;

            if (CUR_TYPE != TOK_RBRACE) {
                item_cap = 4;
                keys = malloc(item_cap * sizeof(char*));
                values = malloc(item_cap * sizeof(ZjnAstNode*));

                /* 键：标识符或字符串 */
                if (CUR_TYPE != TOK_IDENTIFIER && CUR_TYPE != TOK_STRING) {
                    ZJN_THROW("字典键必须是标识符或字符串",
                              parser->current->line, parser->current->column);
                }
                keys[item_count] = (CUR_TYPE == TOK_IDENTIFIER)
                    ? strdup(parser->current->data.ident_val)
                    : strdup(parser->current->data.string_val);
                advance_parser(parser);

                expect(parser, TOK_COLON);
                values[item_count] = parse_expression(parser);
                item_count++;

                while (CUR_TYPE == TOK_COMMA) {
                    advance_parser(parser);
                    skip_newlines(parser); /* 括号内允许换行 */
                    if (CUR_TYPE == TOK_RBRACE) break; /* 允许尾随逗号 */
                    if (item_count >= item_cap) {
                        item_cap *= 2;
                        keys = realloc(keys, item_cap * sizeof(char*));
                        values = realloc(values, item_cap * sizeof(ZjnAstNode*));
                    }
                    if (CUR_TYPE != TOK_IDENTIFIER && CUR_TYPE != TOK_STRING) {
                        ZJN_THROW("字典键必须是标识符或字符串",
                                  parser->current->line, parser->current->column);
                    }
                    keys[item_count] = (CUR_TYPE == TOK_IDENTIFIER)
                        ? strdup(parser->current->data.ident_val)
                        : strdup(parser->current->data.string_val);
                    advance_parser(parser);
                    expect(parser, TOK_COLON);
                    values[item_count] = parse_expression(parser);
                    item_count++;
                }
            }
            skip_newlines(parser);
            expect(parser, TOK_RBRACE);
            return zjn_ast_dict_lit(keys, values, item_count, line, col);
        }
        default:
            ZJN_THROW("不期望的 Token",
                      parser->current ? parser->current->line : 0,
                      parser->current ? parser->current->column : 0);
            return NULL;
    }
}