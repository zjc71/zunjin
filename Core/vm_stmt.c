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
 * zunjin 语言 - 语句执行实现
 * 拆分自 vm.c：代码块与各种语句的执行逻辑
 * ========================================================================== */

#include "vm.h"

/* ===================================================================
 *  代码块执行
 * =================================================================== */

void exec_block(ZjnVM* vm, ZjnAstNode** stmts, int count,
                ZjnEnv* env, LoopControl* loop_ctrl, ReturnValue* ret) {
    if (!stmts) return;
    for (int i = 0; i < count; i++) {
        exec_stmt(vm, stmts[i], env, loop_ctrl, ret);
        if (*loop_ctrl != LOOP_NONE || ret->has_value) return;
    }
}

/* ===================================================================
 *  语句执行
 * =================================================================== */

void exec_stmt(ZjnVM* vm, ZjnAstNode* stmt, ZjnEnv* env,
               LoopControl* loop_ctrl, ReturnValue* ret) {
    if (!stmt) return;

    switch (stmt->type) {
        case ZNODE_EXPR_STMT: {
            ZjnValue* v = eval_expr(vm, stmt->data.expr_stmt.expr, env);
            zjn_val_free(v);
            break;
        }

        /* 变量赋值：env 内部 copy 持有，此处释放本次求值引用 */
        case ZNODE_ASSIGN_STMT: {
            ZjnValue* value = eval_expr(vm, stmt->data.assign.value, env);
            if (!value) value = zjn_val_none();
            zjn_env_define_var(env, stmt->data.assign.name, value);
            zjn_val_free(value);
            break;
        }

        /* 复合赋值：x op= expr 等价 x = x op expr */
        case ZNODE_OP_ASSIGN_STMT: {
            ZjnValue* old = zjn_env_get_var(env, stmt->data.op_assign.name);
            if (!old) {
                ZJN_THROW("变量未定义，无法使用复合赋值", stmt->line, stmt->column);
            }
            ZjnValue* rhs = eval_expr(vm, stmt->data.op_assign.value, env);
            if (!rhs) rhs = zjn_val_none();
            ZjnValue* result = eval_binary_op(vm, stmt->data.op_assign.op,
                                              zjn_val_copy(old), rhs, stmt);
            zjn_val_free(rhs);
            zjn_env_define_var(env, stmt->data.op_assign.name, result);
            zjn_val_free(result);
            break;
        }

        /* 索引/键赋值：obj[key] = value */
        case ZNODE_INDEX_ASSIGN_STMT: {
            ZjnValue* obj = eval_expr(vm, stmt->data.index_assign.obj, env);
            ZjnValue* idx = eval_expr(vm, stmt->data.index_assign.index, env);
            ZjnValue* value = eval_expr(vm, stmt->data.index_assign.value, env);
            if (ZJN_IS_DICT(obj)) {
                if (!ZJN_IS_STRING(idx)) {
                    zjn_val_free(obj); zjn_val_free(idx); zjn_val_free(value);
                    ZJN_THROW("字典键必须是字符串", stmt->line, stmt->column);
                }
                zjn_dict_set(obj, idx->data.string_val, zjn_val_copy(value));
            } else if (ZJN_IS_LIST(obj)) {
                ZjnList* lst = obj->data.list_val;
                int i = zjn_normalize_index((int)zjn_val_to_int(idx), lst->count);
                if (i < 0) {
                    zjn_val_free(obj); zjn_val_free(idx); zjn_val_free(value);
                    ZJN_THROW("列表索引越界", stmt->line, stmt->column);
                }
                zjn_val_free(lst->items[i]);
                lst->items[i] = zjn_val_copy(value);
            } else {
                ZJN_THROW("仅支持对列表和字典的元素赋值", stmt->line, stmt->column);
            }
            zjn_val_free(obj);
            zjn_val_free(idx);
            zjn_val_free(value);
            break;
        }

        case ZNODE_IF_STMT: {
            ZjnValue* cond = eval_expr(vm, stmt->data.if_stmt.condition, env);
            int t = zjn_val_is_truthy(cond);
            zjn_val_free(cond);
            if (t) {
                exec_block(vm, stmt->data.if_stmt.then_body->data.block.stmts,
                           stmt->data.if_stmt.then_body->data.block.stmt_count,
                           env, loop_ctrl, ret);
            } else {
                int done = 0;
                for (int i = 0; i < stmt->data.if_stmt.elif_count; i++) {
                    ZjnElifPair* pair = &stmt->data.if_stmt.elif_pairs[i];
                    cond = eval_expr(vm, pair->condition, env);
                    t = zjn_val_is_truthy(cond);
                    zjn_val_free(cond);
                    if (t) {
                        exec_block(vm, pair->body->data.block.stmts,
                                   pair->body->data.block.stmt_count,
                                   env, loop_ctrl, ret);
                        done = 1;
                        break;
                    }
                }
                if (!done && stmt->data.if_stmt.else_body) {
                    exec_block(vm, stmt->data.if_stmt.else_body->data.block.stmts,
                               stmt->data.if_stmt.else_body->data.block.stmt_count,
                               env, loop_ctrl, ret);
                }
            }
            break;
        }

        case ZNODE_WHILE_STMT: {
            LoopControl inner = LOOP_NONE;
            while (1) {
                ZjnValue* cond = eval_expr(vm, stmt->data.while_stmt.condition, env);
                int t = zjn_val_is_truthy(cond);
                zjn_val_free(cond);
                if (!t) break;
                exec_block(vm, stmt->data.while_stmt.body->data.block.stmts,
                           stmt->data.while_stmt.body->data.block.stmt_count,
                           env, &inner, ret);
                if (inner == LOOP_BREAK) break;
                if (ret->has_value) break;
                inner = LOOP_NONE;
            }
            break;
        }

        case ZNODE_FOR_STMT: {
            ZjnValue* iterable = eval_expr(vm, stmt->data.for_stmt.iterable, env);
            if (!iterable) iterable = zjn_val_none();
            const char* var = stmt->data.for_stmt.var_name;
            ZjnAstNode* body = stmt->data.for_stmt.body;

            if (ZJN_IS_LIST(iterable)) {
                ZjnList* lst = iterable->data.list_val;
                for (int i = 0; i < lst->count; i++) {
                    zjn_env_define_var(env, var, lst->items[i]);
                    LoopControl inner = LOOP_NONE;
                    exec_block(vm, body->data.block.stmts,
                               body->data.block.stmt_count, env, &inner, ret);
                    if (inner == LOOP_BREAK) break;
                    if (ret->has_value) break;
                }
            } else if (ZJN_IS_STRING(iterable)) {
                const char* s = iterable->data.string_val;
                for (size_t i = 0; s[i] != '\0'; i++) {
                    char ch[2] = { s[i], '\0' };
                    ZjnValue* cv = zjn_val_string(ch);
                    zjn_env_define_var(env, var, cv);
                    zjn_val_free(cv);
                    LoopControl inner = LOOP_NONE;
                    exec_block(vm, body->data.block.stmts,
                               body->data.block.stmt_count, env, &inner, ret);
                    if (inner == LOOP_BREAK) break;
                    if (ret->has_value) break;
                }
            } else if (ZJN_IS_DICT(iterable)) {
                /* 遍历字典 = 遍历键 */
                ZjnDict* d = iterable->data.dict_val;
                for (int i = 0; i < d->count; i++) {
                    ZjnValue* kv = zjn_val_string(d->entries[i].key);
                    zjn_env_define_var(env, var, kv);
                    zjn_val_free(kv);
                    LoopControl inner = LOOP_NONE;
                    exec_block(vm, body->data.block.stmts,
                               body->data.block.stmt_count, env, &inner, ret);
                    if (inner == LOOP_BREAK) break;
                    if (ret->has_value) break;
                }
            } else {
                zjn_val_free(iterable);
                ZJN_THROW("无法迭代此类型的值", stmt->line, stmt->column);
            }
            zjn_val_free(iterable);
            break;
        }

        /* 函数定义：函数对象借用 AST 指针，由环境统一托管生命周期 */
        case ZNODE_FUNC_DEF: {
            ZjnFunc* fn = ZUNJIN_ALLOC(ZjnFunc);
            if (!fn) ZJN_THROW("内存不足", stmt->line, stmt->column);
            fn->name = strdup(stmt->data.func_def.name);
            fn->params = stmt->data.func_def.params;
            fn->param_count = stmt->data.func_def.param_count;
            fn->defaults = stmt->data.func_def.defaults;
            fn->body = stmt->data.func_def.body;
            /* 顶层/模块级函数闭包为当前环境；函数体内嵌套定义不捕获
             * 瞬态帧（避免悬垂），沿用全局环境 */
            fn->closure = (vm->call_depth == 0) ? env : vm->global_env;
            ZjnValue* fv = zjn_val_func(fn);
            zjn_env_set_func(env, stmt->data.func_def.name, fv);
            break;
        }

        case ZNODE_RETURN_STMT: {
            if (stmt->data.return_stmt.value) {
                ret->value = eval_expr(vm, stmt->data.return_stmt.value, env);
            } else {
                ret->value = zjn_val_none();
            }
            ret->has_value = 1;
            break;
        }

        case ZNODE_BREAK_STMT:
            *loop_ctrl = LOOP_BREAK;
            break;

        case ZNODE_CONTINUE_STMT:
            *loop_ctrl = LOOP_CONTINUE;
            break;

        case ZNODE_PASS_STMT:
            break;

        case ZNODE_IMPORT_STMT:
            zjn_vm_import_module(vm, env, stmt->data.import_stmt.module_name,
                                 stmt->line, stmt->column);
            break;

        /* 异常处理：jmp_buf 是数组类型不能赋值，须 memcpy 保存/恢复 */
        case ZNODE_TRY_STMT: {
            jmp_buf saved;
            memcpy(saved, zjn_error_buf, sizeof(jmp_buf));
            int saved_depth = vm->call_depth;
            int caught = 0;

            ZJN_TRY() {
                exec_block(vm, stmt->data.try_stmt.try_body->data.block.stmts,
                           stmt->data.try_stmt.try_body->data.block.stmt_count,
                           env, loop_ctrl, ret);
            }
            ZJN_CATCH() {
                caught = 1;
                /* 异常跳转跳过了调用链的深度递减，恢复现场 */
                vm->call_depth = saved_depth;
            }
            ZJN_ENDTRY;

            /* 恢复外层错误上下文：catch/finally 中的异常传播到外层 */
            memcpy(zjn_error_buf, saved, sizeof(jmp_buf));

            if (caught && stmt->data.try_stmt.catch_body) {
                if (stmt->data.try_stmt.catch_var) {
                    ZjnValue* ev = zjn_val_string(zjn_last_error.message);
                    zjn_env_define_var(env, stmt->data.try_stmt.catch_var, ev);
                    zjn_val_free(ev);
                }
                exec_block(vm, stmt->data.try_stmt.catch_body->data.block.stmts,
                           stmt->data.try_stmt.catch_body->data.block.stmt_count,
                           env, loop_ctrl, ret);
            }

            if (stmt->data.try_stmt.finally_body) {
                exec_block(vm, stmt->data.try_stmt.finally_body->data.block.stmts,
                           stmt->data.try_stmt.finally_body->data.block.stmt_count,
                           env, loop_ctrl, ret);
            }
            break;
        }

        case ZNODE_RAISE_STMT: {
            if (stmt->data.raise_stmt.value) {
                ZjnValue* v = eval_expr(vm, stmt->data.raise_stmt.value, env);
                const char* msg = zjn_output_format_value(vm->output, v);
                zjn_val_free(v);
                ZJN_THROW(msg ? msg : "未捕获的异常", stmt->line, stmt->column);
            }
            ZJN_THROW("运行时错误", stmt->line, stmt->column);
            break;
        }

        default:
            /* 表达式位置出现的语句节点（如函数字面量）按表达式求值 */
            {
                ZjnValue* v = eval_expr(vm, stmt, env);
                zjn_val_free(v);
            }
            break;
    }
}