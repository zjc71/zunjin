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
 * zunjin 语言 - AST 节点管理实现
 * 负责 AST 节点的创建、释放
 * 完全独立重新实现
 * ========================================================================== */

#include "ast.h"
#include <string.h>
#include <stdio.h>

/* ---------- AST 创建辅助 ---------- */
static ZjnAstNode* alloc_node(ZjnNodeType type, int line, int col) {
    ZjnAstNode* node = ZUNJIN_ALLOC(ZjnAstNode);
    if (!node) return NULL;
    memset(node, 0, sizeof(ZjnAstNode));
    node->type = type;
    node->line = line;
    node->column = col;
    return node;
}

/* ===================================================================
 *  各类节点创建
 * =================================================================== */

ZjnAstNode* zjn_ast_program(ZjnAstNode** stmts, int count) {
    ZjnAstNode* node = alloc_node(ZNODE_PROGRAM, 0, 0);
    if (!node) return NULL;
    node->data.program.stmts = stmts;
    node->data.program.stmt_count = count;
    return node;
}

ZjnAstNode* zjn_ast_block(ZjnAstNode** stmts, int count) {
    ZjnAstNode* node = alloc_node(ZNODE_BLOCK, 0, 0);
    if (!node) return NULL;
    node->data.block.stmts = stmts;
    node->data.block.stmt_count = count;
    return node;
}

ZjnAstNode* zjn_ast_expr_stmt(ZjnAstNode* expr, int line, int col) {
    ZjnAstNode* node = alloc_node(ZNODE_EXPR_STMT, line, col);
    if (!node) return NULL;
    node->data.expr_stmt.expr = expr;
    return node;
}

ZjnAstNode* zjn_ast_assign(const char* name, ZjnAstNode* value, int line, int col) {
    ZjnAstNode* node = alloc_node(ZNODE_ASSIGN_STMT, line, col);
    if (!node) return NULL;
    node->data.assign.name = strdup(name);
    node->data.assign.value = value;
    return node;
}

ZjnAstNode* zjn_ast_op_assign(const char* name, ZjnOp op,
                              ZjnAstNode* value, int line, int col) {
    ZjnAstNode* node = alloc_node(ZNODE_OP_ASSIGN_STMT, line, col);
    if (!node) return NULL;
    node->data.op_assign.name = strdup(name);
    node->data.op_assign.op = op;
    node->data.op_assign.value = value;
    return node;
}

ZjnAstNode* zjn_ast_index_assign(ZjnAstNode* obj, ZjnAstNode* index,
                                 ZjnAstNode* value, int line, int col) {
    ZjnAstNode* node = alloc_node(ZNODE_INDEX_ASSIGN_STMT, line, col);
    if (!node) return NULL;
    node->data.index_assign.obj = obj;
    node->data.index_assign.index = index;
    node->data.index_assign.value = value;
    return node;
}

ZjnAstNode* zjn_ast_if_stmt(ZjnAstNode* cond, ZjnAstNode* then_body,
                              ZjnElifPair* elifs, int elif_count,
                              ZjnAstNode* else_body, int line, int col) {
    ZjnAstNode* node = alloc_node(ZNODE_IF_STMT, line, col);
    if (!node) return NULL;
    node->data.if_stmt.condition = cond;
    node->data.if_stmt.then_body = then_body;
    node->data.if_stmt.elif_pairs = elifs;
    node->data.if_stmt.elif_count = elif_count;
    node->data.if_stmt.else_body = else_body;
    return node;
}

ZjnAstNode* zjn_ast_while_stmt(ZjnAstNode* cond, ZjnAstNode* body, int line, int col) {
    ZjnAstNode* node = alloc_node(ZNODE_WHILE_STMT, line, col);
    if (!node) return NULL;
    node->data.while_stmt.condition = cond;
    node->data.while_stmt.body = body;
    return node;
}

ZjnAstNode* zjn_ast_for_stmt(const char* var, ZjnAstNode* iterable,
                               ZjnAstNode* body, int line, int col) {
    ZjnAstNode* node = alloc_node(ZNODE_FOR_STMT, line, col);
    if (!node) return NULL;
    node->data.for_stmt.var_name = strdup(var);
    node->data.for_stmt.iterable = iterable;
    node->data.for_stmt.body = body;
    return node;
}

ZjnAstNode* zjn_ast_func_def(const char* name, char** params, int param_count,
                             ZjnAstNode** defaults, ZjnAstNode* body,
                             int line, int col) {
    ZjnAstNode* node = alloc_node(ZNODE_FUNC_DEF, line, col);
    if (!node) return NULL;
    node->data.func_def.name = strdup(name);
    node->data.func_def.params = params;
    node->data.func_def.param_count = param_count;
    node->data.func_def.defaults = defaults;
    node->data.func_def.body = body;
    return node;
}

ZjnAstNode* zjn_ast_return_stmt(ZjnAstNode* value, int line, int col) {
    ZjnAstNode* node = alloc_node(ZNODE_RETURN_STMT, line, col);
    if (!node) return NULL;
    node->data.return_stmt.value = value;
    return node;
}

ZjnAstNode* zjn_ast_break_stmt(int line, int col) {
    return alloc_node(ZNODE_BREAK_STMT, line, col);
}

ZjnAstNode* zjn_ast_continue_stmt(int line, int col) {
    return alloc_node(ZNODE_CONTINUE_STMT, line, col);
}

ZjnAstNode* zjn_ast_pass_stmt(int line, int col) {
    return alloc_node(ZNODE_PASS_STMT, line, col);
}

ZjnAstNode* zjn_ast_import_stmt(const char* module_name, int line, int col) {
    ZjnAstNode* node = alloc_node(ZNODE_IMPORT_STMT, line, col);
    if (!node) return NULL;
    node->data.import_stmt.module_name = strdup(module_name);
    return node;
}

ZjnAstNode* zjn_ast_try_stmt(ZjnAstNode* try_body, const char* catch_var,
                             ZjnAstNode* catch_body, ZjnAstNode* finally_body,
                             int line, int col) {
    ZjnAstNode* node = alloc_node(ZNODE_TRY_STMT, line, col);
    if (!node) return NULL;
    node->data.try_stmt.try_body = try_body;
    node->data.try_stmt.catch_var = catch_var ? strdup(catch_var) : NULL;
    node->data.try_stmt.catch_body = catch_body;
    node->data.try_stmt.finally_body = finally_body;
    return node;
}

ZjnAstNode* zjn_ast_raise_stmt(ZjnAstNode* value, int line, int col) {
    ZjnAstNode* node = alloc_node(ZNODE_RAISE_STMT, line, col);
    if (!node) return NULL;
    node->data.raise_stmt.value = value;
    return node;
}

ZjnAstNode* zjn_ast_binary_op(ZjnOp op, ZjnAstNode* left,
                                ZjnAstNode* right, int line, int col) {
    ZjnAstNode* node = alloc_node(ZNODE_BINARY_OP, line, col);
    if (!node) return NULL;
    node->data.binary.op = op;
    node->data.binary.left = left;
    node->data.binary.right = right;
    return node;
}

ZjnAstNode* zjn_ast_unary_op(ZjnOp op, ZjnAstNode* operand, int line, int col) {
    ZjnAstNode* node = alloc_node(ZNODE_UNARY_OP, line, col);
    if (!node) return NULL;
    node->data.unary.op = op;
    node->data.unary.operand = operand;
    return node;
}

ZjnAstNode* zjn_ast_call_expr(ZjnAstNode* callee,
                                ZjnAstNode** args, int arg_count, int line, int col) {
    ZjnAstNode* node = alloc_node(ZNODE_CALL_EXPR, line, col);
    if (!node) return NULL;
    node->data.call.callee = callee;
    node->data.call.args = args;
    node->data.call.arg_count = arg_count;
    return node;
}

ZjnAstNode* zjn_ast_index_expr(ZjnAstNode* obj, ZjnAstNode* index, int line, int col) {
    ZjnAstNode* node = alloc_node(ZNODE_INDEX_EXPR, line, col);
    if (!node) return NULL;
    node->data.index_expr.obj = obj;
    node->data.index_expr.index = index;
    return node;
}

ZjnAstNode* zjn_ast_slice_expr(ZjnAstNode* obj, ZjnAstNode* start,
                               ZjnAstNode* stop, ZjnAstNode* step,
                               int line, int col) {
    ZjnAstNode* node = alloc_node(ZNODE_SLICE_EXPR, line, col);
    if (!node) return NULL;
    node->data.slice_expr.obj = obj;
    node->data.slice_expr.start = start;
    node->data.slice_expr.stop = stop;
    node->data.slice_expr.step = step;
    return node;
}

ZjnAstNode* zjn_ast_attr_expr(ZjnAstNode* obj, const char* attr,
                              int line, int col) {
    ZjnAstNode* node = alloc_node(ZNODE_ATTR_EXPR, line, col);
    if (!node) return NULL;
    node->data.attr_expr.obj = obj;
    node->data.attr_expr.attr = strdup(attr);
    return node;
}

ZjnAstNode* zjn_ast_variable(const char* name, int line, int col) {
    ZjnAstNode* node = alloc_node(ZNODE_VARIABLE, line, col);
    if (!node) return NULL;
    node->data.variable.name = strdup(name);
    return node;
}

ZjnAstNode* zjn_ast_number_lit(long int_val, double float_val, int is_float, int line, int col) {
    ZjnAstNode* node = alloc_node(ZNODE_NUMBER_LIT, line, col);
    if (!node) return NULL;
    node->data.literal.data.int_val = int_val;
    node->data.literal.data.float_val = float_val;
    node->data.literal.is_float = is_float;
    return node;
}

ZjnAstNode* zjn_ast_string_lit(const char* val, int line, int col) {
    ZjnAstNode* node = alloc_node(ZNODE_STRING_LIT, line, col);
    if (!node) return NULL;
    node->data.literal.data.string_val = strdup(val);
    return node;
}

ZjnAstNode* zjn_ast_bool_lit(int val, int line, int col) {
    ZjnAstNode* node = alloc_node(ZNODE_BOOL_LIT, line, col);
    if (!node) return NULL;
    node->data.literal.data.bool_val = val;
    return node;
}

ZjnAstNode* zjn_ast_none_lit(int line, int col) {
    return alloc_node(ZNODE_NONE_LIT, line, col);
}

ZjnAstNode* zjn_ast_list_lit(ZjnAstNode** items, int count, int line, int col) {
    ZjnAstNode* node = alloc_node(ZNODE_LIST_LIT, line, col);
    if (!node) return NULL;
    node->data.list_lit.items = items;
    node->data.list_lit.item_count = count;
    return node;
}

ZjnAstNode* zjn_ast_dict_lit(char** keys, ZjnAstNode** values, int count,
                             int line, int col) {
    ZjnAstNode* node = alloc_node(ZNODE_DICT_LIT, line, col);
    if (!node) return NULL;
    node->data.dict_lit.keys = keys;
    node->data.dict_lit.values = values;
    node->data.dict_lit.item_count = count;
    return node;
}

/* ===================================================================
 *  elif 对管理
 * =================================================================== */

ZjnElifPair* zjn_ast_elif_pair(ZjnAstNode* cond, ZjnAstNode* body) {
    ZjnElifPair* pair = ZUNJIN_ALLOC(ZjnElifPair);
    if (!pair) return NULL;
    pair->condition = cond;
    pair->body = body;
    return pair;
}

void zjn_ast_elif_pair_free(ZjnElifPair* pair) {
    if (!pair) return;
    if (pair->condition) zjn_ast_free(pair->condition);
    if (pair->body) zjn_ast_free(pair->body);
}

/* ===================================================================
 *  AST 释放
 * =================================================================== */

void zjn_ast_free(ZjnAstNode* node) {
    if (!node) return;

    switch (node->type) {
        case ZNODE_PROGRAM:
            for (int i = 0; i < node->data.program.stmt_count; i++)
                zjn_ast_free(node->data.program.stmts[i]);
            free(node->data.program.stmts);
            break;
        case ZNODE_BLOCK:
            for (int i = 0; i < node->data.block.stmt_count; i++)
                zjn_ast_free(node->data.block.stmts[i]);
            free(node->data.block.stmts);
            break;
        case ZNODE_EXPR_STMT:
            zjn_ast_free(node->data.expr_stmt.expr);
            break;
        case ZNODE_ASSIGN_STMT:
            if (node->data.assign.name) free(node->data.assign.name);
            zjn_ast_free(node->data.assign.value);
            break;
        case ZNODE_OP_ASSIGN_STMT:
            if (node->data.op_assign.name) free(node->data.op_assign.name);
            zjn_ast_free(node->data.op_assign.value);
            break;
        case ZNODE_INDEX_ASSIGN_STMT:
            zjn_ast_free(node->data.index_assign.obj);
            zjn_ast_free(node->data.index_assign.index);
            zjn_ast_free(node->data.index_assign.value);
            break;
        case ZNODE_IF_STMT:
            zjn_ast_free(node->data.if_stmt.condition);
            zjn_ast_free(node->data.if_stmt.then_body);
            for (int i = 0; i < node->data.if_stmt.elif_count; i++)
                zjn_ast_elif_pair_free(&node->data.if_stmt.elif_pairs[i]);
            free(node->data.if_stmt.elif_pairs);
            if (node->data.if_stmt.else_body)
                zjn_ast_free(node->data.if_stmt.else_body);
            break;
        case ZNODE_WHILE_STMT:
            zjn_ast_free(node->data.while_stmt.condition);
            zjn_ast_free(node->data.while_stmt.body);
            break;
        case ZNODE_FOR_STMT:
            if (node->data.for_stmt.var_name) free(node->data.for_stmt.var_name);
            zjn_ast_free(node->data.for_stmt.iterable);
            zjn_ast_free(node->data.for_stmt.body);
            break;
        case ZNODE_FUNC_DEF:
            if (node->data.func_def.name) free(node->data.func_def.name);
            if (node->data.func_def.params) {
                for (int i = 0; i < node->data.func_def.param_count; i++)
                    if (node->data.func_def.params[i]) free(node->data.func_def.params[i]);
                free(node->data.func_def.params);
            }
            if (node->data.func_def.defaults) {
                for (int i = 0; i < node->data.func_def.param_count; i++)
                    if (node->data.func_def.defaults[i])
                        zjn_ast_free(node->data.func_def.defaults[i]);
                free(node->data.func_def.defaults);
            }
            zjn_ast_free(node->data.func_def.body);
            break;
        case ZNODE_RETURN_STMT:
            if (node->data.return_stmt.value)
                zjn_ast_free(node->data.return_stmt.value);
            break;
        case ZNODE_IMPORT_STMT:
            if (node->data.import_stmt.module_name)
                free(node->data.import_stmt.module_name);
            break;
        case ZNODE_TRY_STMT:
            zjn_ast_free(node->data.try_stmt.try_body);
            if (node->data.try_stmt.catch_var) free(node->data.try_stmt.catch_var);
            if (node->data.try_stmt.catch_body) zjn_ast_free(node->data.try_stmt.catch_body);
            if (node->data.try_stmt.finally_body) zjn_ast_free(node->data.try_stmt.finally_body);
            break;
        case ZNODE_RAISE_STMT:
            if (node->data.raise_stmt.value)
                zjn_ast_free(node->data.raise_stmt.value);
            break;
        case ZNODE_PASS_STMT:
        case ZNODE_BREAK_STMT:
        case ZNODE_CONTINUE_STMT:
            break;
        case ZNODE_BINARY_OP:
            zjn_ast_free(node->data.binary.left);
            zjn_ast_free(node->data.binary.right);
            break;
        case ZNODE_UNARY_OP:
            zjn_ast_free(node->data.unary.operand);
            break;
        case ZNODE_CALL_EXPR:
            zjn_ast_free(node->data.call.callee);
            for (int i = 0; i < node->data.call.arg_count; i++)
                zjn_ast_free(node->data.call.args[i]);
            free(node->data.call.args);
            break;
        case ZNODE_INDEX_EXPR:
            zjn_ast_free(node->data.index_expr.obj);
            zjn_ast_free(node->data.index_expr.index);
            break;
        case ZNODE_SLICE_EXPR:
            zjn_ast_free(node->data.slice_expr.obj);
            if (node->data.slice_expr.start) zjn_ast_free(node->data.slice_expr.start);
            if (node->data.slice_expr.stop) zjn_ast_free(node->data.slice_expr.stop);
            if (node->data.slice_expr.step) zjn_ast_free(node->data.slice_expr.step);
            break;
        case ZNODE_ATTR_EXPR:
            zjn_ast_free(node->data.attr_expr.obj);
            if (node->data.attr_expr.attr) free(node->data.attr_expr.attr);
            break;
        case ZNODE_VARIABLE:
            if (node->data.variable.name) free(node->data.variable.name);
            break;
        case ZNODE_STRING_LIT:
            if (node->data.literal.data.string_val)
                free(node->data.literal.data.string_val);
            break;
        case ZNODE_LIST_LIT:
            for (int i = 0; i < node->data.list_lit.item_count; i++)
                zjn_ast_free(node->data.list_lit.items[i]);
            free(node->data.list_lit.items);
            break;
        case ZNODE_DICT_LIT:
            for (int i = 0; i < node->data.dict_lit.item_count; i++) {
                if (node->data.dict_lit.keys[i]) free(node->data.dict_lit.keys[i]);
                zjn_ast_free(node->data.dict_lit.values[i]);
            }
            free(node->data.dict_lit.keys);
            free(node->data.dict_lit.values);
            break;
        default:
            break;
    }
    free(node);
}