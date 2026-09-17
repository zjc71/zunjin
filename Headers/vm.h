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
 * zunjin 语言 - 虚拟机执行器接口
 * AST 解释执行
 * 完全独立重新实现
 *
 * 版本 3.0 变更：
 *  - 新增模块系统：use 语句加载 .z 模块并缓存（模块缓存等价于
 *    模块缓存表（独立实现），支持 obj.attr 属性访问。
 *
 * 版本 4.0 变更：
 *  - 模块化拆分：vm.c 拆分为 vm_core.c / vm_stmt.c / vm_expr.c / vm_module.c
 * ========================================================================== */

#ifndef ZUNJIN_VM_H
#define ZUNJIN_VM_H

#include "zunjin.h"
#include "ast.h"
#include "env.h"
#include "output.h"

/* 递归调用深度上限（防 C 栈溢出崩溃） */
#define ZJN_MAX_CALL_DEPTH 512

/* 模块缓存容量 */
#define ZJN_MAX_MODULES 128

/* ---------- 循环控制标志 ---------- */
typedef enum {
    LOOP_NONE,
    LOOP_BREAK,
    LOOP_CONTINUE,
} LoopControl;

/* ---------- 返回值标志 ---------- */
typedef struct {
    int has_value;
    ZjnValue* value;
} ReturnValue;

/* ---------- 虚拟机 ---------- */
struct ZjnVM {
    ZjnEnv* global_env;
    ZjnOutput* output;
    ZjnError error;
    int call_depth;          /* 当前调用深度 */
    /* 模块缓存：已加载模块值（等价 sys.modules，避免重复执行） */
    ZjnValue* modules[ZJN_MAX_MODULES];
    int module_count;
};

/* ---------- 公开 API ---------- */
ZjnVM* zjn_vm_create(ZjnOutput* output);
void zjn_vm_free(ZjnVM* vm);
int zjn_vm_execute(ZjnVM* vm, ZjnAstNode* program);

/* ---------- 模块系统 ---------- */
/* 加载（或复用缓存中的）模块并绑定到当前环境；
 * 返回 0 成功，非 0 失败（错误信息写入 vm->output） */
int zjn_vm_import_module(ZjnVM* vm, ZjnEnv* env, const char* name,
                         int line, int col);

/* ---------- 内部函数（跨编译单元共享） ---------- */
void exec_stmt(ZjnVM* vm, ZjnAstNode* stmt, ZjnEnv* env,
               LoopControl* loop_ctrl, ReturnValue* ret);
void exec_block(ZjnVM* vm, ZjnAstNode** stmts, int count,
                ZjnEnv* env, LoopControl* loop_ctrl,
                ReturnValue* ret);
ZjnValue* eval_expr(ZjnVM* vm, ZjnAstNode* node, ZjnEnv* env);
ZjnValue* eval_binary_op(ZjnVM* vm, ZjnOp op, ZjnValue* left,
                         ZjnValue* right, ZjnAstNode* node);
void slice_bounds(ZjnValue* start, ZjnValue* stop, ZjnValue* step,
                  int len, int* out_start, int* out_stop,
                  int* out_step, ZjnAstNode* node);

/* ---------- 辅助函数 ---------- */
const char* type_name(ZjnValueType t);

#endif /* ZUNJIN_VM_H */
