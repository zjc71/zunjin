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
 * zunjin 语言 - 表达式求值实现
 * 拆分自 vm.c：切片参数归一化、表达式求值、二元运算
 * ========================================================================== */

#include "vm.h"
#include <math.h>

/* ===================================================================
 *  切片参数归一化（zunjin 语义：负索引 + 长度，nil 用缺省值）
 * =================================================================== */

void slice_bounds(ZjnValue* start, ZjnValue* stop, ZjnValue* step,
                  int len, int* out_start, int* out_stop,
                  int* out_step, ZjnAstNode* node) {
    int ln = node ? node->line : 0;
    int col = node ? node->column : 0;
    int s = 1;
    if (step) {
        s = (int)zjn_val_to_int(step);
        if (s == 0) ZJN_THROW("切片步长不能为 0", ln, col);
    }
    *out_step = s;
    if (s > 0) {
        int st = 0, sp = len;
        if (start) {
            st = (int)zjn_val_to_int(start);
            if (st < 0) st += len;
            if (st < 0) st = 0;
            if (st > len) st = len;
        }
        if (stop) {
            sp = (int)zjn_val_to_int(stop);
            if (sp < 0) sp += len;
            if (sp < 0) sp = 0;
            if (sp > len) sp = len;
        }
        *out_start = st;
        *out_stop = sp;
    } else {
        int st = len - 1, sp = -1;
        if (start) {
            st = (int)zjn_val_to_int(start);
            if (st < 0) st += len;
            if (st < 0) st = -1;
            if (st >= len) st = len - 1;
        }
        if (stop) {
            sp = (int)zjn_val_to_int(stop);
            if (sp < 0) sp += len;
            if (sp < -1) sp = -1;
            if (sp >= len) sp = len - 1;
        }
        *out_start = st;
        *out_stop = sp;
    }
}

/* ===================================================================
 *  表达式求值
 * =================================================================== */

ZjnValue* eval_expr(ZjnVM* vm, ZjnAstNode* node, ZjnEnv* env) {
    if (!node) return zjn_val_none();

    switch (node->type) {
        case ZNODE_NUMBER_LIT:
            return node->data.literal.is_float
                ? zjn_val_float(node->data.literal.data.float_val)
                : zjn_val_int(node->data.literal.data.int_val);

        case ZNODE_STRING_LIT:
            return zjn_val_string(node->data.literal.data.string_val);

        case ZNODE_BOOL_LIT:
            return zjn_val_bool(node->data.literal.data.bool_val);

        case ZNODE_NONE_LIT:
            return zjn_val_none();

        case ZNODE_VARIABLE: {
            /* 先查变量链，再查函数链（put/proc 定义等） */
            ZjnValue* v = zjn_env_get_var(env, node->data.variable.name);
            if (!v) v = zjn_env_get_func(env, node->data.variable.name);
            if (!v) {
                ZJN_THROW("变量未定义", node->line, node->column);
            }
            return zjn_val_copy(v);
        }

        case ZNODE_LIST_LIT: {
            ZjnValue* lst = zjn_val_list();
            for (int i = 0; i < node->data.list_lit.item_count; i++) {
                ZjnValue* item = eval_expr(vm, node->data.list_lit.items[i], env);
                zjn_list_append(lst, item);
            }
            return lst;
        }

        case ZNODE_DICT_LIT: {
            ZjnValue* d = zjn_val_dict();
            for (int i = 0; i < node->data.dict_lit.item_count; i++) {
                ZjnValue* v = eval_expr(vm, node->data.dict_lit.values[i], env);
                zjn_dict_set(d, node->data.dict_lit.keys[i], v);
            }
            return d;
        }

        /* 函数字面量（表达式中定义函数，注册后返回空值） */
        case ZNODE_FUNC_DEF: {
            ZjnFunc* fn = ZUNJIN_ALLOC(ZjnFunc);
            if (!fn) ZJN_THROW("内存不足", node->line, node->column);
            fn->name = strdup(node->data.func_def.name);
            fn->params = node->data.func_def.params;
            fn->param_count = node->data.func_def.param_count;
            fn->defaults = node->data.func_def.defaults;
            fn->body = node->data.func_def.body;
            fn->closure = (vm->call_depth == 0) ? env : vm->global_env;
            ZjnValue* fv = zjn_val_func(fn);
            zjn_env_set_func(env, node->data.func_def.name, fv);
            return zjn_val_none();
        }

        /* 二元运算：both/either 短路求值，其余委托 eval_binary_op */
        case ZNODE_BINARY_OP: {
            ZjnOp op = node->data.binary.op;
            if (op == ZOP_AND || op == ZOP_OR) {
                ZjnValue* left = eval_expr(vm, node->data.binary.left, env);
                int lt = zjn_val_is_truthy(left);
                if (op == ZOP_AND) {
                    if (!lt) return left;   /* 短路：返回左操作数 */
                    zjn_val_free(left);
                    return eval_expr(vm, node->data.binary.right, env);
                } else {
                    if (lt) return left;    /* 短路：返回左操作数 */
                    zjn_val_free(left);
                    return eval_expr(vm, node->data.binary.right, env);
                }
            }
            ZjnValue* left = eval_expr(vm, node->data.binary.left, env);
            ZjnValue* right = eval_expr(vm, node->data.binary.right, env);
            ZjnValue* result = eval_binary_op(vm, op, left, right, node);
            zjn_val_free(left);
            zjn_val_free(right);
            return result;
        }

        case ZNODE_UNARY_OP: {
            ZjnValue* v = eval_expr(vm, node->data.unary.operand, env);
            ZjnOp op = node->data.unary.op;
            if (op == ZOP_NEG) {
                if (ZJN_IS_INT(v)) { v->data.int_val = -v->data.int_val; return v; }
                if (ZJN_IS_FLOAT(v)) { v->data.float_val = -v->data.float_val; return v; }
                zjn_val_free(v);
                ZJN_THROW("一元负号只能用于数值", node->line, node->column);
            }
            if (op == ZOP_BNOT) {
                if (ZJN_IS_INT(v)) { v->data.int_val = ~v->data.int_val; return v; }
                zjn_val_free(v);
                ZJN_THROW("按位取反只能用于整数", node->line, node->column);
            }
            if (op == ZOP_NOT) {
                int t = zjn_val_is_truthy(v);
                zjn_val_free(v);
                return zjn_val_bool(!t);
            }
            zjn_val_free(v);
            return zjn_val_none();
        }

        case ZNODE_INDEX_EXPR: {
            ZjnValue* obj = eval_expr(vm, node->data.index_expr.obj, env);
            ZjnValue* idx = eval_expr(vm, node->data.index_expr.index, env);
            ZjnValue* result = NULL;
            if (ZJN_IS_DICT(obj)) {
                if (!ZJN_IS_STRING(idx)) {
                    zjn_val_free(obj); zjn_val_free(idx);
                    ZJN_THROW("字典键必须是字符串", node->line, node->column);
                }
                ZjnValue* got = zjn_dict_get(obj, idx->data.string_val);
                result = got ? zjn_val_copy(got) : zjn_val_none();
            } else if (ZJN_IS_LIST(obj) || ZJN_IS_STRING(obj)) {
                result = zjn_val_index(obj, (int)zjn_val_to_int(idx));
                if (!result) {
                    zjn_val_free(obj); zjn_val_free(idx);
                    ZJN_THROW("索引越界", node->line, node->column);
                }
            } else {
                zjn_val_free(obj); zjn_val_free(idx);
                char msg[128];
                snprintf(msg, sizeof(msg), "不能对%s类型进行索引",
                         type_name(obj->type));
                ZJN_THROW(msg, node->line, node->column);
            }
            zjn_val_free(obj);
            zjn_val_free(idx);
            return result;
        }

        case ZNODE_SLICE_EXPR: {
            ZjnValue* obj = eval_expr(vm, node->data.slice_expr.obj, env);
            if (!ZJN_IS_LIST(obj) && !ZJN_IS_STRING(obj)) {
                zjn_val_free(obj);
                ZJN_THROW("切片只能用于列表或字符串", node->line, node->column);
            }
            ZjnValue* start = node->data.slice_expr.start
                ? eval_expr(vm, node->data.slice_expr.start, env) : NULL;
            ZjnValue* stop = node->data.slice_expr.stop
                ? eval_expr(vm, node->data.slice_expr.stop, env) : NULL;
            ZjnValue* step = node->data.slice_expr.step
                ? eval_expr(vm, node->data.slice_expr.step, env) : NULL;
            int len = ZJN_IS_LIST(obj)
                ? obj->data.list_val->count
                : (int)strlen(obj->data.string_val);
            int s, e, st;
            slice_bounds(start, stop, step, len, &s, &e, &st, node);
            ZjnValue* result = zjn_val_slice(obj, s, e, st);
            zjn_val_free(obj);
            if (start) zjn_val_free(start);
            if (stop) zjn_val_free(stop);
            if (step) zjn_val_free(step);
            return result;
        }

        /* 属性访问：obj.attr（模块成员读取） */
        case ZNODE_ATTR_EXPR: {
            ZjnValue* obj = eval_expr(vm, node->data.attr_expr.obj, env);
            if (!ZJN_IS_MODULE(obj)) {
                zjn_val_free(obj);
                ZJN_THROW("属性访问仅支持模块对象（obj.attr）",
                          node->line, node->column);
            }
            ZjnModule* m = obj->data.module_val;
            ZjnValue* member = zjn_env_get_var(m->ns, node->data.attr_expr.attr);
            if (!member)
                member = zjn_env_get_func(m->ns, node->data.attr_expr.attr);
            /* 模块未定义该成员时回退到全局环境（内置函数/全局符号），
             * 等价 zunjin 中模块可访问全局内建 */
            if (!member) {
                member = zjn_env_get_func(vm->global_env, node->data.attr_expr.attr);
            }
            if (!member) {
                zjn_val_free(obj);
                char msg[256];
                snprintf(msg, sizeof(msg), "模块 %s 没有成员 %s",
                         m->name, node->data.attr_expr.attr);
                ZJN_THROW(msg, node->line, node->column);
            }
            ZjnValue* result = zjn_val_copy(member);
            zjn_val_free(obj);
            return result;
        }

        case ZNODE_CALL_EXPR: {
            ZjnValue* callee = eval_expr(vm, node->data.call.callee, env);
            if (!callee) return zjn_val_none();
            if (!ZJN_IS_FUNC(callee) && !ZJN_IS_BUILTIN(callee)) {
                zjn_val_free(callee);
                ZJN_THROW("不能调用非函数类型", node->line, node->column);
            }

            int argc = node->data.call.arg_count;
            ZjnValue** args = NULL;
            if (argc > 0) {
                args = ZUNJIN_ALLOC_N(ZjnValue*, argc);
                if (!args) {
                    zjn_val_free(callee);
                    ZJN_THROW("内存不足", node->line, node->column);
                }
                for (int i = 0; i < argc; i++)
                    args[i] = eval_expr(vm, node->data.call.args[i], env);
            }

            ZjnValue* result = NULL;

            if (ZJN_IS_BUILTIN(callee)) {
                ZjnBuiltin* b = callee->data.builtin_val;
                if (b->arity >= 0 && argc != b->arity) {
                    for (int i = 0; i < argc; i++) zjn_val_free(args[i]);
                    free(args);
                    zjn_val_free(callee);
                    char msg[160];
                    snprintf(msg, sizeof(msg),
                             "内置函数 %s 需要 %d 个参数，实际 %d 个",
                             b->name, b->arity, argc);
                    ZJN_THROW(msg, node->line, node->column);
                }
                result = b->func(argc, args);
                if (!result) result = zjn_val_none();
            } else {
                ZjnFunc* fn = callee->data.func_val;

                /* 参数数量校验：不多不少 */
                if (argc > fn->param_count) {
                    for (int i = 0; i < argc; i++) zjn_val_free(args[i]);
                    free(args);
                    zjn_val_free(callee);
                    char msg[160];
                    snprintf(msg, sizeof(msg),
                             "函数 %s 最多接受 %d 个参数，实际 %d 个",
                             fn->name, fn->param_count, argc);
                    ZJN_THROW(msg, node->line, node->column);
                }
                int required = fn->param_count;
                while (required > 0 && fn->defaults &&
                       fn->defaults[required - 1]) required--;
                if (argc < required) {
                    for (int i = 0; i < argc; i++) zjn_val_free(args[i]);
                    free(args);
                    zjn_val_free(callee);
                    char msg[160];
                    snprintf(msg, sizeof(msg),
                             "函数 %s 缺少必要参数（需要至少 %d 个）",
                             fn->name, required);
                    ZJN_THROW(msg, node->line, node->column);
                }

                /* 递归深度限制：防 C 栈溢出崩溃 */
                if (vm->call_depth >= ZJN_MAX_CALL_DEPTH) {
                    for (int i = 0; i < argc; i++) zjn_val_free(args[i]);
                    free(args);
                    zjn_val_free(callee);
                    ZJN_THROW("递归深度超过限制（可能导致栈溢出）",
                              node->line, node->column);
                }

                vm->call_depth++;
                /* 函数帧父环境 = 闭包环境（模块函数可见模块命名空间） */
                ZjnEnv* func_env = zjn_env_push(
                    fn->closure ? fn->closure : vm->global_env, fn->name);
                for (int i = 0; i < fn->param_count; i++) {
                    if (i < argc) {
                        /* 所有权转移：环境直接持有实参，避免 copy/free 往返 */
                        zjn_env_define_var_take(func_env, fn->params[i], args[i]);
                        args[i] = NULL; /* 已接管，调用方不再释放 */
                    } else if (fn->defaults && fn->defaults[i]) {
                        ZjnValue* dv = eval_expr(vm, fn->defaults[i], func_env);
                        zjn_env_define_var_take(func_env, fn->params[i], dv);
                    } else {
                        zjn_env_define_var_take(func_env, fn->params[i],
                                                zjn_val_none());
                    }
                }
                LoopControl lc = LOOP_NONE;
                ReturnValue fr = { 0, NULL };
                exec_block(vm, fn->body->data.block.stmts,
                           fn->body->data.block.stmt_count,
                           func_env, &lc, &fr);
                zjn_env_free(func_env);
                vm->call_depth--;
                result = fr.has_value ? fr.value : zjn_val_none();
            }

            for (int i = 0; i < argc; i++) {
                if (args[i]) zjn_val_free(args[i]); /* 仅释放未被接管的 */
            }
            free(args);
            zjn_val_free(callee);
            return result;
        }

        default:
            return zjn_val_none();
    }
}

/* ===================================================================
 *  二元运算求值
 *  返回新值（refs=1）；left/right 由调用者释放
 * =================================================================== */

ZjnValue* eval_binary_op(ZjnVM* vm, ZjnOp op, ZjnValue* left,
                        ZjnValue* right, ZjnAstNode* node) {
    (void)vm;
    int ln = node ? node->line : 0;
    int col = node ? node->column : 0;

    /* ---------- 比较运算 ---------- */
    if (op == ZOP_EQ || op == ZOP_NEQ || op == ZOP_LT ||
        op == ZOP_LE || op == ZOP_GT || op == ZOP_GE) {
        int r;
        if (op == ZOP_EQ) {
            r = zjn_val_equal(left, right);
        } else if (op == ZOP_NEQ) {
            r = !zjn_val_equal(left, right);
        } else {
            int c = zjn_val_compare(left, right);
            if (op == ZOP_LT)       r = c < 0;
            else if (op == ZOP_LE)  r = c <= 0;
            else if (op == ZOP_GT)  r = c > 0;
            else                    r = c >= 0;
        }
        return zjn_val_bool(r);
    }

    /* ---------- 成员测试：x of y → y 是容器 ---------- */
    if (op == ZOP_IN || op == ZOP_NOT_IN) {
        int r = zjn_val_contains(right, left);
        if (op == ZOP_NOT_IN) r = !r;
        return zjn_val_bool(r);
    }

    /* ---------- 加法：数值 / 字符串拼接 / 列表拼接 ---------- */
    if (op == ZOP_ADD) {
        if (ZJN_IS_STRING(left) && ZJN_IS_STRING(right))
            return zjn_string_concat(left, right);
        if (ZJN_IS_LIST(left) && ZJN_IS_LIST(right))
            return zjn_list_concat(left, right);
        if (!ZJN_IS_NUMBER(left) || !ZJN_IS_NUMBER(right)) {
            ZJN_THROW("加法操作数必须是数值、字符串或列表", ln, col);
        }
        if (ZJN_IS_INT(left) && ZJN_IS_INT(right)) {
            long r;
            if (__builtin_add_overflow(left->data.int_val,
                                       right->data.int_val, &r)) {
                return zjn_val_float((double)left->data.int_val +
                                     (double)right->data.int_val);
            }
            return zjn_val_int(r);
        }
        return zjn_val_float(zjn_val_to_float(left) + zjn_val_to_float(right));
    }

    /* ---------- 减法 ---------- */
    if (op == ZOP_SUB) {
        if (!ZJN_IS_NUMBER(left) || !ZJN_IS_NUMBER(right)) {
            ZJN_THROW("减法操作数必须是数值", ln, col);
        }
        if (ZJN_IS_INT(left) && ZJN_IS_INT(right)) {
            long r;
            if (__builtin_sub_overflow(left->data.int_val,
                                       right->data.int_val, &r)) {
                return zjn_val_float((double)left->data.int_val -
                                     (double)right->data.int_val);
            }
            return zjn_val_int(r);
        }
        return zjn_val_float(zjn_val_to_float(left) - zjn_val_to_float(right));
    }

    /* ---------- 乘法：数值 / 字符串与列表重复 ---------- */
    if (op == ZOP_MUL) {
        if (ZJN_IS_STRING(left) && ZJN_IS_INT(right)) {
            if (right->data.int_val < 0) ZJN_THROW("字符串重复次数不能为负", ln, col);
            return zjn_string_repeat(left, right->data.int_val);
        }
        if (ZJN_IS_INT(left) && ZJN_IS_STRING(right)) {
            if (left->data.int_val < 0) ZJN_THROW("字符串重复次数不能为负", ln, col);
            return zjn_string_repeat(right, left->data.int_val);
        }
        if (ZJN_IS_LIST(left) && ZJN_IS_INT(right)) {
            if (right->data.int_val < 0) ZJN_THROW("列表重复次数不能为负", ln, col);
            return zjn_list_repeat(left, right->data.int_val);
        }
        if (ZJN_IS_INT(left) && ZJN_IS_LIST(right)) {
            if (left->data.int_val < 0) ZJN_THROW("列表重复次数不能为负", ln, col);
            return zjn_list_repeat(right, left->data.int_val);
        }
        if (!ZJN_IS_NUMBER(left) || !ZJN_IS_NUMBER(right)) {
            ZJN_THROW("乘法操作数必须是数值、字符串或列表", ln, col);
        }
        if (ZJN_IS_INT(left) && ZJN_IS_INT(right)) {
            long r;
            if (__builtin_mul_overflow(left->data.int_val,
                                       right->data.int_val, &r)) {
                return zjn_val_float((double)left->data.int_val *
                                     (double)right->data.int_val);
            }
            return zjn_val_int(r);
        }
        return zjn_val_float(zjn_val_to_float(left) * zjn_val_to_float(right));
    }

    /* ---------- 除法：恒为浮点（zunjin 语义） ---------- */
    if (op == ZOP_DIV) {
        if (!ZJN_IS_NUMBER(left) || !ZJN_IS_NUMBER(right)) {
            ZJN_THROW("除法操作数必须是数值", ln, col);
        }
        if (zjn_val_to_float(right) == 0.0) {
            ZJN_THROW("除数为零", ln, col);
        }
        return zjn_val_float(zjn_val_to_float(left) / zjn_val_to_float(right));
    }

    /* ---------- 整除：向下取整（zunjin 语义） ---------- */
    if (op == ZOP_FLOORDIV) {
        if (!ZJN_IS_NUMBER(left) || !ZJN_IS_NUMBER(right)) {
            ZJN_THROW("整除操作数必须是数值", ln, col);
        }
        if (zjn_val_to_float(right) == 0.0) {
            ZJN_THROW("除数为零", ln, col);
        }
        if (ZJN_IS_INT(left) && ZJN_IS_INT(right)) {
            long a = left->data.int_val, b = right->data.int_val;
            long q = a / b, r = a % b;
            if (r != 0 && ((r < 0) != (b < 0))) q--;   /* 向负无穷取整 */
            return zjn_val_int(q);
        }
        return zjn_val_float(floor(zjn_val_to_float(left) /
                                   zjn_val_to_float(right)));
    }

    /* ---------- 取模：结果符号与除数一致（zunjin 语义） ---------- */
    if (op == ZOP_MOD) {
        if (!ZJN_IS_NUMBER(left) || !ZJN_IS_NUMBER(right)) {
            ZJN_THROW("取模操作数必须是数值", ln, col);
        }
        if (zjn_val_to_float(right) == 0.0) {
            ZJN_THROW("除数为零", ln, col);
        }
        if (ZJN_IS_INT(left) && ZJN_IS_INT(right)) {
            long a = left->data.int_val, b = right->data.int_val;
            long m = a % b;
            if (m != 0 && ((m < 0) != (b < 0))) m += b;
            return zjn_val_int(m);
        }
        double a = zjn_val_to_float(left), b = zjn_val_to_float(right);
        double m = fmod(a, b);
        if (m != 0.0 && ((m < 0.0) != (b < 0.0))) m += b;
        return zjn_val_float(m);
    }

    /* ---------- 幂运算：整数指数非负且结果可表示时保持整数 ---------- */
    if (op == ZOP_POW) {
        if (!ZJN_IS_NUMBER(left) || !ZJN_IS_NUMBER(right)) {
            ZJN_THROW("幂运算操作数必须是数值", ln, col);
        }
        if (zjn_val_to_float(left) == 0.0 && zjn_val_to_float(right) < 0.0) {
            ZJN_THROW("零的负数次幂", ln, col);
        }
        if (ZJN_IS_INT(left) && ZJN_IS_INT(right) && right->data.int_val >= 0) {
            long base = left->data.int_val;
            unsigned long exp = (unsigned long)right->data.int_val;
            long result = 1;
            int overflow = 0;
            while (exp > 0) {
                if (exp & 1) {
                    if (__builtin_mul_overflow(result, base, &result)) {
                        overflow = 1;
                        break;
                    }
                }
                exp >>= 1;
                if (exp > 0) {
                    if (__builtin_mul_overflow(base, base, &base)) {
                        overflow = 1;
                        break;
                    }
                }
            }
            if (!overflow) return zjn_val_int(result);
        }
        return zjn_val_float(pow(zjn_val_to_float(left), zjn_val_to_float(right)));
    }

    /* ---------- 位运算：按位与 / 或 / 异或 / 左移 / 右移 ---------- */
    if (op == ZOP_BAND || op == ZOP_BOR ||
        op == ZOP_BXOR || op == ZOP_SHL ||
        op == ZOP_SHR) {
        if (!ZJN_IS_INT(left) || !ZJN_IS_INT(right)) {
            ZJN_THROW("位运算操作数必须是整数", ln, col);
        }
        long a = left->data.int_val, b = right->data.int_val;
        if (op == ZOP_BAND)  return zjn_val_int(a & b);
        if (op == ZOP_BOR)   return zjn_val_int(a | b);
        if (op == ZOP_BXOR)  return zjn_val_int(a ^ b);
        if (b < 0 || b >= 64) {
            ZJN_THROW("移位量必须是非负且小于 64", ln, col);
        }
        if (op == ZOP_SHL) return zjn_val_int(a << b);
        return zjn_val_int(a >> b);
    }

    ZJN_THROW("不支持的运算符", ln, col);
    return zjn_val_none();
}