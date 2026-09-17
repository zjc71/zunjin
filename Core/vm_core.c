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
 * zunjin 语言 - 虚拟机核心实现
 * VM 创建/销毁、执行入口
 * 拆分自 vm.c：VM 核心职责
 * ========================================================================== */

#include "vm.h"
#include "builtins.h"
#include "lexer.h"
#include "parser.h"

/* ---------- 类型名（错误提示用） ---------- */
const char* type_name(ZjnValueType t) {
    switch (t) {
        case ZVAL_NONE:    return "null";
        case ZVAL_BOOL:    return "布尔";
        case ZVAL_INT:     return "整数";
        case ZVAL_FLOAT:   return "浮点数";
        case ZVAL_STRING:  return "字符串";
        case ZVAL_LIST:    return "列表";
        case ZVAL_DICT:    return "字典";
        case ZVAL_FUNC:    return "函数";
        case ZVAL_BUILTIN: return "内置函数";
        case ZVAL_MODULE:  return "模块";
    }
    return "未知";
}

/* ===================================================================
 *  VM 创建/销毁
 * =================================================================== */

ZjnVM* zjn_vm_create(ZjnOutput* output) {
    ZjnVM* vm = ZUNJIN_ALLOC(ZjnVM);
    if (!vm) return NULL;
    vm->output = output;
    vm->call_depth = 0;
    vm->module_count = 0;
    vm->global_env = zjn_env_create_global();
    if (!vm->global_env) {
        free(vm);
        return NULL;
    }
    /* 内置函数 print 等经输出模块输出，保证 IDE 捕获 */
    zjn_builtins_set_output(output);
    zjn_builtins_register_all(vm->global_env);
    memset(&vm->error, 0, sizeof(ZjnError));
    return vm;
}

void zjn_vm_free(ZjnVM* vm) {
    if (!vm) return;
    if (vm->global_env) zjn_env_free(vm->global_env);
    /* 释放模块缓存（模块值托管各自命名空间与 AST） */
    for (int i = 0; i < vm->module_count; i++)
        if (vm->modules[i]) zjn_val_free(vm->modules[i]);
    free(vm);
}

/* ===================================================================
 *  执行入口
 * =================================================================== */

int zjn_vm_execute(ZjnVM* vm, ZjnAstNode* program) {
    if (!program || program->type != ZNODE_PROGRAM) {
        zjn_output_error(vm->output, "无效的 AST 根节点");
        return 1;
    }

    LoopControl loop_ctrl = LOOP_NONE;
    ReturnValue ret = { 0, NULL };

    ZJN_TRY() {
        exec_block(vm, program->data.program.stmts,
                   program->data.program.stmt_count,
                   vm->global_env, &loop_ctrl, &ret);
    }
    ZJN_CATCH() {
        zjn_output_error(vm->output, "%s (行 %d, 列 %d)",
                         zjn_last_error.message,
                         zjn_last_error.line,
                         zjn_last_error.column);
        return 1;
    }
    ZJN_ENDTRY;

    if (ret.value) zjn_val_free(ret.value);
    return 0;
}