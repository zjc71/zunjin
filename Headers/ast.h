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
 * zunjin 语言 - 抽象语法树 (AST) 节点定义
 * 完全独立重新实现
 *
 * 版本 2.0 新增节点：
 *  - OP_ASSIGN：复合赋值（+= -= *= /= //= %= **=）
 *  - INDEX_ASSIGN：索引/字典键赋值（obj[key] = value）
 *  - DICT_LIT：字典字面量
 *  - SLICE_EXPR：切片表达式（含步长，start/stop/step 可为空）
 *  - TRY_STMT / RAISE_STMT：异常处理
 *  - FUNC_DEF 增加默认参数表
 * ========================================================================== */

#ifndef ZUNJIN_AST_H
#define ZUNJIN_AST_H

#include "zunjin.h"

/* ---------- 运算符枚举（消除运行期 strcmp 开销，等价于字节码操作码设计） ---------- */
typedef enum {
    ZOP_NONE = 0,
    /* 算术 */
    ZOP_ADD,        /* + */
    ZOP_SUB,        /* - */
    ZOP_MUL,        /* * */
    ZOP_DIV,        /* / */
    ZOP_FLOORDIV,   /* // */
    ZOP_MOD,        /* % */
    ZOP_POW,        /* ** */
    /* 比较 */
    ZOP_EQ,         /* == */
    ZOP_NEQ,        /* != */
    ZOP_LT,         /* < */
    ZOP_GT,         /* > */
    ZOP_LE,         /* <= */
    ZOP_GE,         /* >= */
    /* 逻辑 */
    ZOP_AND,        /* both */
    ZOP_OR,         /* either */
    ZOP_NOT,        /* negate（一元） */
    /* 成员 */
    ZOP_IN,         /* of */
    ZOP_NOT_IN,     /* negate of */
    /* 位运算 */
    ZOP_BAND,       /* & */
    ZOP_BOR,        /* | */
    ZOP_BXOR,       /* ^ */
    ZOP_SHL,        /* << */
    ZOP_SHR,        /* >> */
    ZOP_BNOT,       /* ~（一元） */
    ZOP_NEG,        /* -（一元） */
} ZjnOp;

/* ---------- AST 节点类型 ---------- */
typedef enum {
    /* 语句 */
    ZNODE_PROGRAM,
    ZNODE_BLOCK,
    ZNODE_EXPR_STMT,
    ZNODE_ASSIGN_STMT,
    ZNODE_OP_ASSIGN_STMT,
    ZNODE_INDEX_ASSIGN_STMT,
    ZNODE_IF_STMT,
    ZNODE_WHILE_STMT,
    ZNODE_FOR_STMT,
    ZNODE_FUNC_DEF,
    ZNODE_RETURN_STMT,
    ZNODE_BREAK_STMT,
    ZNODE_CONTINUE_STMT,
    ZNODE_PASS_STMT,
    ZNODE_IMPORT_STMT,
    ZNODE_TRY_STMT,
    ZNODE_RAISE_STMT,

    /* 表达式 */
    ZNODE_BINARY_OP,
    ZNODE_UNARY_OP,
    ZNODE_CALL_EXPR,
    ZNODE_INDEX_EXPR,
    ZNODE_SLICE_EXPR,
    ZNODE_ATTR_EXPR,
    ZNODE_VARIABLE,

    /* 字面量 */
    ZNODE_NUMBER_LIT,
    ZNODE_STRING_LIT,
    ZNODE_BOOL_LIT,
    ZNODE_NONE_LIT,
    ZNODE_LIST_LIT,
    ZNODE_DICT_LIT,
} ZjnNodeType;

/* ---------- 前向声明 ---------- */
typedef struct ZjnAstNode ZjnAstNode;

/* ---------- elif 分支 ---------- */
typedef struct ZjnElifPair {
    ZjnAstNode* condition;
    ZjnAstNode* body;       /* Block */
} ZjnElifPair;

/* ---------- AST 节点 ---------- */
struct ZjnAstNode {
    ZjnNodeType type;
    int line;
    int column;

    union {
        /* Program: 语句列表 */
        struct {
            ZjnAstNode** stmts;
            int stmt_count;
        } program;

        /* Block: 代码块 */
        struct {
            ZjnAstNode** stmts;
            int stmt_count;
        } block;

        /* ExprStmt: 表达式语句 */
        struct {
            ZjnAstNode* expr;
        } expr_stmt;

        /* AssignStmt: name = expr */
        struct {
            char* name;
            ZjnAstNode* value;
        } assign;

        /* OpAssignStmt: name op= expr */
        struct {
            char* name;
            ZjnOp op;          /* ZOP_ADD/ZOP_SUB/... */
            ZjnAstNode* value;
        } op_assign;

        /* IndexAssignStmt: obj[index] = expr */
        struct {
            ZjnAstNode* obj;
            ZjnAstNode* index;
            ZjnAstNode* value;
        } index_assign;

        /* IfStmt: if/elif/else */
        struct {
            ZjnAstNode* condition;
            ZjnAstNode* then_body;       /* Block */
            ZjnElifPair* elif_pairs;
            int elif_count;
            ZjnAstNode* else_body;       /* Block or NULL */
        } if_stmt;

        /* WhileStmt: while cond: body */
        struct {
            ZjnAstNode* condition;
            ZjnAstNode* body;
        } while_stmt;

        /* ForStmt: for var in iterable: body */
        struct {
            char* var_name;
            ZjnAstNode* iterable;
            ZjnAstNode* body;
        } for_stmt;

        /* FuncDef: func name(params): body */
        struct {
            char* name;
            char** params;
            int param_count;
            ZjnAstNode** defaults;   /* 与 params 等长；无默认项为 NULL */
            ZjnAstNode* body;
        } func_def;

        /* ReturnStmt */
        struct {
            ZjnAstNode* value;   /* 可为 NULL */
        } return_stmt;

        /* ImportStmt: import module_name */
        struct {
            char* module_name;
        } import_stmt;

        /* TryStmt: attempt: body seize [var]: body settle: body */
        struct {
            ZjnAstNode* try_body;       /* Block */
            char* catch_var;            /* 可为 NULL */
            ZjnAstNode* catch_body;     /* Block or NULL */
            ZjnAstNode* finally_body;   /* Block or NULL */
        } try_stmt;

        /* RaiseStmt: fling [expr] */
        struct {
            ZjnAstNode* value;   /* 可为 NULL */
        } raise_stmt;

        /* BinaryOp: left op right */
        struct {
            ZjnOp op;
            ZjnAstNode* left;
            ZjnAstNode* right;
        } binary;

        /* UnaryOp: op operand */
        struct {
            ZjnOp op;
            ZjnAstNode* operand;
        } unary;

        /* CallExpr: callee(args) */
        struct {
            ZjnAstNode* callee;
            ZjnAstNode** args;
            int arg_count;
        } call;

        /* IndexExpr: obj[index] */
        struct {
            ZjnAstNode* obj;
            ZjnAstNode* index;
        } index_expr;

        /* SliceExpr: obj[start:stop:step]（字段可为 NULL） */
        struct {
            ZjnAstNode* obj;
            ZjnAstNode* start;
            ZjnAstNode* stop;
            ZjnAstNode* step;
        } slice_expr;

        /* AttrExpr: obj.attr（模块成员访问） */
        struct {
            ZjnAstNode* obj;
            char* attr;
        } attr_expr;

        /* Variable: 变量引用 */
        struct {
            char* name;
        } variable;

        /* 字面量 */
        struct {
            union {
                long     int_val;
                double   float_val;
                char*    string_val;
                int      bool_val;
            } data;
            int is_float;
        } literal;

        /* ListLit: [expr, expr, ...] */
        struct {
            ZjnAstNode** items;
            int item_count;
        } list_lit;

        /* DictLit: {key: expr, ...} */
        struct {
            char** keys;
            ZjnAstNode** values;
            int item_count;
        } dict_lit;
    } data;
};

/* ---------- AST 创建函数 ---------- */
ZjnAstNode* zjn_ast_program(ZjnAstNode** stmts, int count);
ZjnAstNode* zjn_ast_block(ZjnAstNode** stmts, int count);
ZjnAstNode* zjn_ast_expr_stmt(ZjnAstNode* expr, int line, int col);
ZjnAstNode* zjn_ast_assign(const char* name, ZjnAstNode* value, int line, int col);
ZjnAstNode* zjn_ast_op_assign(const char* name, ZjnOp op,
                              ZjnAstNode* value, int line, int col);
ZjnAstNode* zjn_ast_index_assign(ZjnAstNode* obj, ZjnAstNode* index,
                                 ZjnAstNode* value, int line, int col);
ZjnAstNode* zjn_ast_if_stmt(ZjnAstNode* cond, ZjnAstNode* then_body,
                             ZjnElifPair* elifs, int elif_count,
                             ZjnAstNode* else_body, int line, int col);
ZjnAstNode* zjn_ast_while_stmt(ZjnAstNode* cond, ZjnAstNode* body, int line, int col);
ZjnAstNode* zjn_ast_for_stmt(const char* var, ZjnAstNode* iterable,
                              ZjnAstNode* body, int line, int col);
ZjnAstNode* zjn_ast_func_def(const char* name, char** params, int param_count,
                             ZjnAstNode** defaults, ZjnAstNode* body,
                             int line, int col);
ZjnAstNode* zjn_ast_return_stmt(ZjnAstNode* value, int line, int col);
ZjnAstNode* zjn_ast_break_stmt(int line, int col);
ZjnAstNode* zjn_ast_continue_stmt(int line, int col);
ZjnAstNode* zjn_ast_pass_stmt(int line, int col);
ZjnAstNode* zjn_ast_import_stmt(const char* module_name, int line, int col);
ZjnAstNode* zjn_ast_try_stmt(ZjnAstNode* try_body, const char* catch_var,
                             ZjnAstNode* catch_body, ZjnAstNode* finally_body,
                             int line, int col);
ZjnAstNode* zjn_ast_raise_stmt(ZjnAstNode* value, int line, int col);
ZjnAstNode* zjn_ast_binary_op(ZjnOp op, ZjnAstNode* left,
                               ZjnAstNode* right, int line, int col);
ZjnAstNode* zjn_ast_unary_op(ZjnOp op, ZjnAstNode* operand, int line, int col);
ZjnAstNode* zjn_ast_call_expr(ZjnAstNode* callee,
                               ZjnAstNode** args, int arg_count, int line, int col);
ZjnAstNode* zjn_ast_index_expr(ZjnAstNode* obj, ZjnAstNode* index, int line, int col);
ZjnAstNode* zjn_ast_slice_expr(ZjnAstNode* obj, ZjnAstNode* start,
                               ZjnAstNode* stop, ZjnAstNode* step,
                               int line, int col);
ZjnAstNode* zjn_ast_attr_expr(ZjnAstNode* obj, const char* attr,
                              int line, int col);
ZjnAstNode* zjn_ast_variable(const char* name, int line, int col);
ZjnAstNode* zjn_ast_number_lit(long int_val, double float_val, int is_float, int line, int col);
ZjnAstNode* zjn_ast_string_lit(const char* val, int line, int col);
ZjnAstNode* zjn_ast_bool_lit(int val, int line, int col);
ZjnAstNode* zjn_ast_none_lit(int line, int col);
ZjnAstNode* zjn_ast_list_lit(ZjnAstNode** items, int count, int line, int col);
ZjnAstNode* zjn_ast_dict_lit(char** keys, ZjnAstNode** values, int count,
                             int line, int col);

/* ---------- AST 释放 ---------- */
void zjn_ast_free(ZjnAstNode* node);

/* ---------- elif 对创建 ---------- */
ZjnElifPair* zjn_ast_elif_pair(ZjnAstNode* cond, ZjnAstNode* body);
void zjn_ast_elif_pair_free(ZjnElifPair* pair);

#endif /* ZUNJIN_AST_H */
