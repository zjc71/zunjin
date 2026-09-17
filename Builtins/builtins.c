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
 * zunjin 语言 - 内置函数模块实现
 * 完全独立重新实现
 * 注册全局可用的内置函数
 *
 * 版本 2.0 变更：
 *  - 输出类函数（put/read 提示符）经输出模块输出。
 *  - upto 支持步长；minimum/maximum/total 支持单列表参数。
 *  - 新增数学、字符串、字典、迭代器组合内置（对照主流脚本语言标准）。
 * ========================================================================== */

#include "builtins.h"
#include <string.h>
#include <stdio.h>
#include <stdlib.h>
#include <math.h>
#include <ctype.h>

#ifdef _WIN32
#include <windows.h>
#endif

/* ---------- 输出模块（print 等输出类内置使用） ---------- */
static ZjnOutput* g_output = NULL;

void zjn_builtins_set_output(ZjnOutput* output) {
    g_output = output;
}

/* ---------- 注册辅助 ---------- */
static void register_builtin(ZjnEnv* env, const char* name,
                              int arity,
                              ZjnValue* (*func)(int, ZjnValue**)) {
    ZjnBuiltin* builtin = ZUNJIN_ALLOC(ZjnBuiltin);
    if (!builtin) return;
    builtin->name = strdup(name);
    builtin->arity = arity;
    builtin->func = func;
    ZjnValue* val = zjn_val_builtin(builtin);
    zjn_env_set_func(env, name, val);
}

/* ---------- 注册所有内置函数 ---------- */
void zjn_builtins_register_all(ZjnEnv* env) {
    /* 基础 I/O */
    register_builtin(env, "put",     -1, zjn_builtin_print);
    register_builtin(env, "read",    -1, zjn_builtin_input);

    /* 序列 */
    register_builtin(env, "upto",    -1, zjn_builtin_range);
    register_builtin(env, "size",     1, zjn_builtin_len);
    register_builtin(env, "array",   -1, zjn_builtin_list);
    register_builtin(env, "push",     2, zjn_builtin_append);
    register_builtin(env, "pop",     -1, zjn_builtin_pop);
    register_builtin(env, "insert",   3, zjn_builtin_insert);
    register_builtin(env, "erase",    2, zjn_builtin_remove);
    register_builtin(env, "delete",   2, zjn_builtin_delete_at);
    register_builtin(env, "clear",    1, zjn_builtin_clear);
    register_builtin(env, "extend",   2, zjn_builtin_extend);
    register_builtin(env, "count",    2, zjn_builtin_count);
    register_builtin(env, "index",    2, zjn_builtin_index_of);

    /* 类型转换 */
    register_builtin(env, "text",     1, zjn_builtin_str);
    register_builtin(env, "integer",  1, zjn_builtin_int);
    register_builtin(env, "decimal",  1, zjn_builtin_float);
    register_builtin(env, "boolean",  1, zjn_builtin_bool);
    register_builtin(env, "typeof",   1, zjn_builtin_type);
    register_builtin(env, "ord",      1, zjn_builtin_ord);
    register_builtin(env, "chr",      1, zjn_builtin_chr);
    register_builtin(env, "hex",      1, zjn_builtin_hex);
    register_builtin(env, "oct",      1, zjn_builtin_oct);
    register_builtin(env, "bin",      1, zjn_builtin_bin);

    /* 数值运算 */
    register_builtin(env, "minimum", -1, zjn_builtin_min);
    register_builtin(env, "maximum", -1, zjn_builtin_max);
    register_builtin(env, "total",   -1, zjn_builtin_sum);
    register_builtin(env, "sum",     -1, zjn_builtin_sum);
    register_builtin(env, "absolute", 1, zjn_builtin_abs);
    register_builtin(env, "round",   -1, zjn_builtin_round);
    register_builtin(env, "power",    2, zjn_builtin_pow);
    register_builtin(env, "root",     1, zjn_builtin_sqrt);
    register_builtin(env, "sqrt",     1, zjn_builtin_sqrt);
    register_builtin(env, "floor",    1, zjn_builtin_floor);
    register_builtin(env, "ceiling",  1, zjn_builtin_ceil);

    /* 排序 */
    register_builtin(env, "sort",     1, zjn_builtin_sorted);
    register_builtin(env, "reverse",  1, zjn_builtin_reversed);

    /* 字符串处理 */
    register_builtin(env, "split",   -1, zjn_builtin_split);
    register_builtin(env, "join",     2, zjn_builtin_join);
    register_builtin(env, "replace",  3, zjn_builtin_replace);
    register_builtin(env, "find",    -1, zjn_builtin_find);
    register_builtin(env, "strip",    1, zjn_builtin_strip);
    register_builtin(env, "lstrip",   1, zjn_builtin_lstrip);
    register_builtin(env, "rstrip",   1, zjn_builtin_rstrip);
    register_builtin(env, "upper",    1, zjn_builtin_upper);
    register_builtin(env, "lower",    1, zjn_builtin_lower);
    register_builtin(env, "startswith", 2, zjn_builtin_startswith);
    register_builtin(env, "endswith", 2, zjn_builtin_endswith);

    /* 迭代器组合 */
    register_builtin(env, "enumerate", 1, zjn_builtin_enumerate);
    register_builtin(env, "zip",       2, zjn_builtin_zip);

    /* 字典 */
    register_builtin(env, "keys",     1, zjn_builtin_keys);
    register_builtin(env, "values",   1, zjn_builtin_values);

#ifdef _WIN32
    /* GUI（窗口程序支持） */
    register_builtin(env, "sleep",      1, zjn_builtin_sleep);
    register_builtin(env, "window",     3, zjn_builtin_window);
    register_builtin(env, "label",      4, zjn_builtin_label);
    register_builtin(env, "button",     6, zjn_builtin_button);
    register_builtin(env, "textbox",    5, zjn_builtin_textbox);
    register_builtin(env, "show",       1, zjn_builtin_show);
    register_builtin(env, "wait_click", 1, zjn_builtin_wait_click);
    register_builtin(env, "is_open",    1, zjn_builtin_is_open);
    register_builtin(env, "close",      1, zjn_builtin_close);
    register_builtin(env, "get_text",   1, zjn_builtin_get_text);
    register_builtin(env, "set_text",   2, zjn_builtin_set_text);
    register_builtin(env, "msgbox",    -1, zjn_builtin_msgbox);
#endif /* _WIN32 */
}

/* ===================================================================
 *  基础 I/O
 * =================================================================== */

/* ---------- print ---------- */
ZjnValue* zjn_builtin_print(int argc, ZjnValue** args) {
    if (g_output) {
        zjn_output_print_value(g_output, args, argc);
    } else {
        for (int i = 0; i < argc; i++) {
            if (i > 0) printf(" ");
            zjn_val_print(args[i], stdout);
        }
        printf("\n");
    }
    return zjn_val_none();
}

/* ---------- input ---------- */
ZjnValue* zjn_builtin_input(int argc, ZjnValue** args) {
    if (argc > 0 && args[0]) {
        if (g_output) {
            const char* s = zjn_output_format_value(g_output, args[0]);
            zjn_output_write(g_output, s);
            zjn_output_flush(g_output);
        } else {
            printf("%s", zjn_val_to_string(args[0]));
        }
    }
    char buf[1024];
    if (!fgets(buf, sizeof(buf), stdin)) {
        return zjn_val_string("");
    }
    size_t len = strlen(buf);
    while (len > 0 && (buf[len - 1] == '\n' || buf[len - 1] == '\r'))
        buf[--len] = '\0';
    return zjn_val_string(buf);
}

/* ===================================================================
 *  序列操作
 * =================================================================== */

/* ---------- range ---------- */
ZjnValue* zjn_builtin_range(int argc, ZjnValue** args) {
    long start = 0, end = 0, step = 1;
    if (argc == 1) {
        end = zjn_val_to_int(args[0]);
    } else if (argc == 2) {
        start = zjn_val_to_int(args[0]);
        end = zjn_val_to_int(args[1]);
    } else if (argc >= 3) {
        start = zjn_val_to_int(args[0]);
        end = zjn_val_to_int(args[1]);
        step = zjn_val_to_int(args[2]);
    } else {
        return zjn_val_none();
    }
    if (step == 0) return zjn_val_list();

    ZjnValue* list = zjn_val_list();
    if (step > 0) {
        for (long i = start; i < end; i += step) {
            zjn_list_append(list, zjn_val_int(i));
        }
    } else {
        for (long i = start; i > end; i += step) {
            zjn_list_append(list, zjn_val_int(i));
        }
    }
    return list;
}

/* ---------- len ---------- */
ZjnValue* zjn_builtin_len(int argc, ZjnValue** args) {
    (void)argc;
    return zjn_val_len(args[0]);
}

/* ---------- list ---------- */
ZjnValue* zjn_builtin_list(int argc, ZjnValue** args) {
    if (argc == 0 || !args[0]) return zjn_val_list();
    ZjnValue* result = zjn_val_list();
    if (ZJN_IS_LIST(args[0])) {
        ZjnList* src = args[0]->data.list_val;
        for (int i = 0; i < src->count; i++)
            zjn_list_append(result, zjn_val_copy(src->items[i]));
    } else if (ZJN_IS_STRING(args[0])) {
        const char* s = args[0]->data.string_val;
        for (size_t i = 0; s[i] != '\0'; i++) {
            char ch[2] = { s[i], '\0' };
            zjn_list_append(result, zjn_val_string(ch));
        }
    } else if (ZJN_IS_DICT(args[0])) {
        ZjnDict* d = args[0]->data.dict_val;
        for (int i = 0; i < d->count; i++)
            zjn_list_append(result, zjn_val_string(d->entries[i].key));
    }
    return result;
}

/* ---------- append ---------- */
ZjnValue* zjn_builtin_append(int argc, ZjnValue** args) {
    (void)argc;
    if (!args[0] || args[0]->type != ZVAL_LIST) return zjn_val_none();
    zjn_list_append(args[0], zjn_val_copy(args[1]));
    return zjn_val_none();
}

/* ---------- pop ---------- */
ZjnValue* zjn_builtin_pop(int argc, ZjnValue** args) {
    if (!args[0] || args[0]->type != ZVAL_LIST) return zjn_val_none();
    ZjnList* lst = args[0]->data.list_val;
    int idx;
    if (argc >= 2) {
        idx = zjn_normalize_index((int)zjn_val_to_int(args[1]), lst->count);
        if (idx < 0) return zjn_val_none();
    } else {
        idx = lst->count - 1;
    }
    if (idx < 0 || lst->count == 0) return zjn_val_none();
    ZjnValue* item = lst->items[idx];
    for (int j = idx; j < lst->count - 1; j++)
        lst->items[j] = lst->items[j + 1];
    lst->count--;
    lst->items[lst->count] = NULL;   /* 防止残留悬垂指针 */
    return item;                     /* 所有权转移给调用者 */
}

/* ---------- insert ---------- */
ZjnValue* zjn_builtin_insert(int argc, ZjnValue** args) {
    (void)argc;
    if (!args[0] || args[0]->type != ZVAL_LIST) return zjn_val_none();
    int index = (int)zjn_val_to_int(args[1]);
    ZjnList* lst = args[0]->data.list_val;
    if (index < 0) index = 0;
    if (index > lst->count) index = lst->count;
    /* 扩容 */
    if (lst->count >= lst->capacity) {
        int new_cap = lst->capacity == 0 ? 8 : lst->capacity * 2;
        ZjnValue** new_items = realloc(lst->items, new_cap * sizeof(ZjnValue*));
        if (!new_items) return zjn_val_none();
        lst->items = new_items;
        lst->capacity = new_cap;
    }
    /* 后移元素 */
    for (int i = lst->count; i > index; i--) {
        lst->items[i] = lst->items[i - 1];
    }
    lst->items[index] = zjn_val_copy(args[2]);
    lst->count++;
    return zjn_val_none();
}

/* ---------- remove：按值删除（保持旧版 erase 语义） ---------- */
ZjnValue* zjn_builtin_remove(int argc, ZjnValue** args) {
    (void)argc;
    if (!args[0] || args[0]->type != ZVAL_LIST) return zjn_val_none();
    ZjnList* lst = args[0]->data.list_val;
    ZjnValue* target = args[1];
    for (int i = 0; i < lst->count; i++) {
        if (zjn_val_equal(lst->items[i], target)) {
            zjn_val_free(lst->items[i]);
            for (int j = i; j < lst->count - 1; j++)
                lst->items[j] = lst->items[j + 1];
            lst->count--;
            lst->items[lst->count] = NULL;   /* 防止残留悬垂指针 */
            break;
        }
    }
    return zjn_val_none();
}

/* ---------- delete：列表按索引删除 / 字典按键删除 ---------- */
ZjnValue* zjn_builtin_delete_at(int argc, ZjnValue** args) {
    (void)argc;
    if (!args[0]) return zjn_val_none();
    if (args[0]->type == ZVAL_DICT) {
        if (ZJN_IS_STRING(args[1]))
            zjn_dict_remove(args[0], args[1]->data.string_val);
        return zjn_val_none();
    }
    if (args[0]->type != ZVAL_LIST) return zjn_val_none();
    ZjnList* lst = args[0]->data.list_val;
    int idx = zjn_normalize_index((int)zjn_val_to_int(args[1]), lst->count);
    if (idx < 0) return zjn_val_none();
    zjn_val_free(lst->items[idx]);
    for (int j = idx; j < lst->count - 1; j++)
        lst->items[j] = lst->items[j + 1];
    lst->count--;
    lst->items[lst->count] = NULL;
    return zjn_val_none();
}

/* ---------- clear：清空列表或字典 ---------- */
ZjnValue* zjn_builtin_clear(int argc, ZjnValue** args) {
    (void)argc;
    if (!args[0]) return zjn_val_none();
    if (args[0]->type == ZVAL_DICT) {
        zjn_dict_clear(args[0]);
        return zjn_val_none();
    }
    if (args[0]->type != ZVAL_LIST) return zjn_val_none();
    ZjnList* lst = args[0]->data.list_val;
    for (int i = 0; i < lst->count; i++) {
        zjn_val_free(lst->items[i]);
        lst->items[i] = NULL;
    }
    lst->count = 0;
    return zjn_val_none();
}

/* ---------- extend ---------- */
ZjnValue* zjn_builtin_extend(int argc, ZjnValue** args) {
    (void)argc;
    if (!args[0] || args[0]->type != ZVAL_LIST) return zjn_val_none();
    ZjnList* dst = args[0]->data.list_val;
    if (ZJN_IS_LIST(args[1])) {
        ZjnList* src = args[1]->data.list_val;
        for (int i = 0; i < src->count; i++)
            zjn_list_append(args[0], zjn_val_copy(src->items[i]));
    } else if (ZJN_IS_STRING(args[1])) {
        const char* s = args[1]->data.string_val;
        for (size_t i = 0; s[i] != '\0'; i++) {
            char ch[2] = { s[i], '\0' };
            zjn_list_append(args[0], zjn_val_string(ch));
        }
    } else if (ZJN_IS_DICT(args[1])) {
        ZjnDict* d = args[1]->data.dict_val;
        for (int i = 0; i < d->count; i++)
            zjn_list_append(args[0], zjn_val_string(d->entries[i].key));
    }
    (void)dst;
    return zjn_val_none();
}

/* ---------- count ---------- */
ZjnValue* zjn_builtin_count(int argc, ZjnValue** args) {
    (void)argc;
    int n = 0;
    if (ZJN_IS_LIST(args[0])) {
        ZjnList* lst = args[0]->data.list_val;
        for (int i = 0; i < lst->count; i++)
            if (zjn_val_equal(lst->items[i], args[1])) n++;
    } else if (ZJN_IS_STRING(args[0]) && ZJN_IS_STRING(args[1])) {
        const char* hay = args[0]->data.string_val;
        const char* needle = args[1]->data.string_val;
        size_t nlen = strlen(needle);
        if (nlen == 0) return zjn_val_int(0);
        const char* p = hay;
        while ((p = strstr(p, needle)) != NULL) {
            n++;
            p += nlen;
        }
    }
    return zjn_val_int(n);
}

/* ---------- index：首次出现位置，找不到返回 -1 ---------- */
ZjnValue* zjn_builtin_index_of(int argc, ZjnValue** args) {
    (void)argc;
    if (ZJN_IS_LIST(args[0])) {
        ZjnList* lst = args[0]->data.list_val;
        for (int i = 0; i < lst->count; i++)
            if (zjn_val_equal(lst->items[i], args[1])) return zjn_val_int(i);
    } else if (ZJN_IS_STRING(args[0]) && ZJN_IS_STRING(args[1])) {
        const char* p = strstr(args[0]->data.string_val, args[1]->data.string_val);
        if (p) return zjn_val_int((long)(p - args[0]->data.string_val));
    }
    return zjn_val_int(-1);
}

/* ===================================================================
 *  类型转换
 * =================================================================== */

/* ---------- str ---------- */
ZjnValue* zjn_builtin_str(int argc, ZjnValue** args) {
    (void)argc;
    if (!args[0]) return zjn_val_string("null");
    switch (args[0]->type) {
        case ZVAL_STRING:
            return zjn_val_copy(args[0]);
        case ZVAL_LIST:
        case ZVAL_DICT: {
            const char* s = zjn_output_format_value(g_output, args[0]);
            return zjn_val_string(s ? s : "<object>");
        }
        default: {
            char buf[256];
            switch (args[0]->type) {
                case ZVAL_NONE:   strcpy(buf, "null"); break;
                case ZVAL_BOOL:   strcpy(buf, args[0]->data.bool_val ? "yes" : "no"); break;
                case ZVAL_INT:    snprintf(buf, sizeof(buf), "%ld", args[0]->data.int_val); break;
                case ZVAL_FLOAT:  snprintf(buf, sizeof(buf), "%g", args[0]->data.float_val); break;
                case ZVAL_FUNC:   snprintf(buf, sizeof(buf), "<函数 %s>", args[0]->data.func_val->name); break;
                default:          strcpy(buf, "<object>"); break;
            }
            return zjn_val_string(buf);
        }
    }
}

/* ---------- int ---------- */
ZjnValue* zjn_builtin_int(int argc, ZjnValue** args) {
    (void)argc;
    if (!args[0]) return zjn_val_int(0);
    return zjn_val_int(zjn_val_to_int(args[0]));
}

/* ---------- float ---------- */
ZjnValue* zjn_builtin_float(int argc, ZjnValue** args) {
    (void)argc;
    if (!args[0]) return zjn_val_float(0.0);
    return zjn_val_float(zjn_val_to_float(args[0]));
}

/* ---------- bool ---------- */
ZjnValue* zjn_builtin_bool(int argc, ZjnValue** args) {
    (void)argc;
    if (!args[0]) return zjn_val_bool(0);
    return zjn_val_bool(zjn_val_is_truthy(args[0]));
}

/* ---------- type ---------- */
ZjnValue* zjn_builtin_type(int argc, ZjnValue** args) {
    (void)argc;
    if (!args[0]) return zjn_val_string("null");
    switch (args[0]->type) {
        case ZVAL_NONE:    return zjn_val_string("null");
        case ZVAL_BOOL:    return zjn_val_string("bool");
        case ZVAL_INT:     return zjn_val_string("int");
        case ZVAL_FLOAT:   return zjn_val_string("float");
        case ZVAL_STRING:  return zjn_val_string("str");
        case ZVAL_LIST:    return zjn_val_string("list");
        case ZVAL_DICT:    return zjn_val_string("dict");
        case ZVAL_FUNC:    return zjn_val_string("function");
        case ZVAL_BUILTIN: return zjn_val_string("builtin_function");
        default:           return zjn_val_string("unknown");
    }
}

/* ---------- ord ---------- */
ZjnValue* zjn_builtin_ord(int argc, ZjnValue** args) {
    (void)argc;
    if (!args[0] || args[0]->type != ZVAL_STRING ||
        args[0]->data.string_val[0] == '\0') {
        return zjn_val_int(0);
    }
    return zjn_val_int((long)(unsigned char)args[0]->data.string_val[0]);
}

/* ---------- chr ---------- */
ZjnValue* zjn_builtin_chr(int argc, ZjnValue** args) {
    (void)argc;
    char buf[2] = { (char)zjn_val_to_int(args[0]), '\0' };
    return zjn_val_string(buf);
}

/* ---------- hex / oct / bin ---------- */
ZjnValue* zjn_builtin_hex(int argc, ZjnValue** args) {
    (void)argc;
    char buf[64];
    snprintf(buf, sizeof(buf), "0x%lx", zjn_val_to_int(args[0]));
    return zjn_val_string(buf);
}

ZjnValue* zjn_builtin_oct(int argc, ZjnValue** args) {
    (void)argc;
    char buf[64];
    snprintf(buf, sizeof(buf), "0o%lo", zjn_val_to_int(args[0]));
    return zjn_val_string(buf);
}

ZjnValue* zjn_builtin_bin(int argc, ZjnValue** args) {
    (void)argc;
    long n = zjn_val_to_int(args[0]);
    char buf[80];
    char* p = buf + sizeof(buf) - 1;
    *p = '\0';
    unsigned long u = (unsigned long)(n < 0 ? -n : n);
    if (u == 0) { *--p = '0'; }
    while (u > 0) { *--p = (char)('0' + (u & 1)); u >>= 1; }
    *--p = 'b';
    *--p = '0';
    if (n < 0) *--p = '-';
    return zjn_val_string(p);
}

/* ===================================================================
 *  数值运算
 * =================================================================== */

/* ---------- min / max：多参数或单列表 ---------- */
ZjnValue* zjn_builtin_min(int argc, ZjnValue** args) {
    if (argc == 0) return zjn_val_none();
    const ZjnValue* best = args[0];
    if (argc == 1 && ZJN_IS_LIST(args[0])) {
        ZjnList* lst = args[0]->data.list_val;
        if (lst->count == 0) return zjn_val_none();
        best = lst->items[0];
        for (int i = 1; i < lst->count; i++)
            if (zjn_val_compare(lst->items[i], best) < 0) best = lst->items[i];
        return zjn_val_copy(best);
    }
    for (int i = 1; i < argc; i++)
        if (zjn_val_compare(args[i], best) < 0) best = args[i];
    return zjn_val_copy(best);
}

ZjnValue* zjn_builtin_max(int argc, ZjnValue** args) {
    if (argc == 0) return zjn_val_none();
    const ZjnValue* best = args[0];
    if (argc == 1 && ZJN_IS_LIST(args[0])) {
        ZjnList* lst = args[0]->data.list_val;
        if (lst->count == 0) return zjn_val_none();
        best = lst->items[0];
        for (int i = 1; i < lst->count; i++)
            if (zjn_val_compare(lst->items[i], best) > 0) best = lst->items[i];
        return zjn_val_copy(best);
    }
    for (int i = 1; i < argc; i++)
        if (zjn_val_compare(args[i], best) > 0) best = args[i];
    return zjn_val_copy(best);
}

/* ---------- sum：多参数或单列表，数值累加 ---------- */
ZjnValue* zjn_builtin_sum(int argc, ZjnValue** args) {
    if (argc == 0) return zjn_val_int(0);
    int is_float = 0, count = 0;
    long total_int = 0;
    double total_float = 0.0;
    ZjnValue** items = args;
    int item_count = argc;
    ZjnList* lst = NULL;
    if (argc == 1 && ZJN_IS_LIST(args[0])) {
        lst = args[0]->data.list_val;
        items = lst->items;
        item_count = lst->count;
    }
    for (int i = 0; i < item_count; i++) {
        ZjnValue* v = items[i];
        if (ZJN_IS_FLOAT(v)) is_float = 1;
        if (ZJN_IS_NUMBER(v)) {
            total_int += zjn_val_to_int(v);
            total_float += zjn_val_to_float(v);
            count++;
        }
    }
    if (count == 0) return zjn_val_int(0);
    return is_float ? zjn_val_float(total_float) : zjn_val_int(total_int);
}

/* ---------- abs ---------- */
ZjnValue* zjn_builtin_abs(int argc, ZjnValue** args) {
    (void)argc;
    if (!args[0]) return zjn_val_int(0);
    if (ZJN_IS_FLOAT(args[0])) {
        double v = args[0]->data.float_val;
        return zjn_val_float(v < 0 ? -v : v);
    }
    long v = zjn_val_to_int(args[0]);
    return zjn_val_int(v < 0 ? -v : v);
}

/* ---------- round ---------- */
ZjnValue* zjn_builtin_round(int argc, ZjnValue** args) {
    if (argc == 0 || !args[0]) return zjn_val_int(0);
    double x = zjn_val_to_float(args[0]);
    if (argc >= 2) {
        int n = (int)zjn_val_to_int(args[1]);
        double scale = pow(10.0, n);
        return zjn_val_float(rint(x * scale) / scale);
    }
    return zjn_val_int((long)rint(x));
}

/* ---------- pow ---------- */
ZjnValue* zjn_builtin_pow(int argc, ZjnValue** args) {
    (void)argc;
    double base = zjn_val_to_float(args[0]);
    double exp = zjn_val_to_float(args[1]);
    return zjn_val_float(pow(base, exp));
}

/* ---------- sqrt ---------- */
ZjnValue* zjn_builtin_sqrt(int argc, ZjnValue** args) {
    (void)argc;
    double x = zjn_val_to_float(args[0]);
    if (x < 0) return zjn_val_none();
    return zjn_val_float(sqrt(x));
}

/* ---------- floor ---------- */
ZjnValue* zjn_builtin_floor(int argc, ZjnValue** args) {
    (void)argc;
    if (ZJN_IS_INT(args[0])) return zjn_val_copy(args[0]);
    return zjn_val_int((long)floor(zjn_val_to_float(args[0])));
}

/* ---------- ceil ---------- */
ZjnValue* zjn_builtin_ceil(int argc, ZjnValue** args) {
    (void)argc;
    if (ZJN_IS_INT(args[0])) return zjn_val_copy(args[0]);
    return zjn_val_int((long)ceil(zjn_val_to_float(args[0])));
}

/* ===================================================================
 *  排序
 * =================================================================== */

/* ---------- sorted ---------- */
ZjnValue* zjn_builtin_sorted(int argc, ZjnValue** args) {
    (void)argc;
    if (!args[0] || args[0]->type != ZVAL_LIST) return zjn_val_list();
    ZjnList* src = args[0]->data.list_val;
    ZjnValue* result = zjn_val_list();
    for (int i = 0; i < src->count; i++)
        zjn_list_append(result, zjn_val_copy(src->items[i]));
    ZjnList* dst = result->data.list_val;
    /* 冒泡排序（按值比较，数值与字符串通用） */
    for (int i = 0; i < dst->count - 1; i++) {
        for (int j = 0; j < dst->count - 1 - i; j++) {
            if (zjn_val_compare(dst->items[j], dst->items[j + 1]) > 0) {
                ZjnValue* tmp = dst->items[j];
                dst->items[j] = dst->items[j + 1];
                dst->items[j + 1] = tmp;
            }
        }
    }
    return result;
}

/* ---------- reversed ---------- */
ZjnValue* zjn_builtin_reversed(int argc, ZjnValue** args) {
    (void)argc;
    if (!args[0]) return zjn_val_list();
    if (ZJN_IS_LIST(args[0])) {
        ZjnList* src = args[0]->data.list_val;
        ZjnValue* result = zjn_val_list();
        for (int i = src->count - 1; i >= 0; i--)
            zjn_list_append(result, zjn_val_copy(src->items[i]));
        return result;
    }
    if (ZJN_IS_STRING(args[0])) {
        const char* s = args[0]->data.string_val;
        size_t len = strlen(s);
        char* buf = (char*)malloc(len + 1);
        if (!buf) return zjn_val_string("");
        for (size_t i = 0; i < len; i++) buf[i] = s[len - 1 - i];
        buf[len] = '\0';
        ZjnValue* v = zjn_val_string(buf);
        free(buf);
        return v;
    }
    return zjn_val_list();
}

/* ===================================================================
 *  字符串处理
 * =================================================================== */

/* ---------- split ---------- */
ZjnValue* zjn_builtin_split(int argc, ZjnValue** args) {
    if (argc == 0 || !args[0] || args[0]->type != ZVAL_STRING)
        return zjn_val_list();
    const char* s = args[0]->data.string_val;
    ZjnValue* result = zjn_val_list();
    if (argc >= 2 && ZJN_IS_STRING(args[1])) {
        /* 按指定分隔符分割 */
        const char* sep = args[1]->data.string_val;
        size_t seplen = strlen(sep);
        if (seplen == 0) {
            for (size_t i = 0; s[i] != '\0'; i++) {
                char ch[2] = { s[i], '\0' };
                zjn_list_append(result, zjn_val_string(ch));
            }
            return result;
        }
        const char* p = s;
        while (1) {
            const char* hit = strstr(p, sep);
            if (!hit) {
                zjn_list_append(result, zjn_val_string(p));
                break;
            }
            char* part = (char*)malloc((size_t)(hit - p) + 1);
            if (part) {
                memcpy(part, p, (size_t)(hit - p));
                part[hit - p] = '\0';
                zjn_list_append(result, zjn_val_string(part));
                free(part);
            }
            p = hit + seplen;
        }
        return result;
    }
    /* 按空白分割（zunjin 标准行为） */
    const char* p = s;
    while (*p) {
        while (*p && isspace((unsigned char)*p)) p++;
        if (!*p) break;
        const char* start = p;
        while (*p && !isspace((unsigned char)*p)) p++;
        char* part = (char*)malloc((size_t)(p - start) + 1);
        if (part) {
            memcpy(part, start, (size_t)(p - start));
            part[p - start] = '\0';
            zjn_list_append(result, zjn_val_string(part));
            free(part);
        }
    }
    return result;
}

/* ---------- join ---------- */
ZjnValue* zjn_builtin_join(int argc, ZjnValue** args) {
    (void)argc;
    const char* sep = zjn_val_to_string(args[0]);
    ZjnValue* result = zjn_val_string("");
    size_t seplen = strlen(sep);
    ZjnValue* parts = args[1];
    if (ZJN_IS_LIST(parts)) {
        ZjnList* lst = parts->data.list_val;
        for (int i = 0; i < lst->count; i++) {
            if (i > 0) {
                ZjnValue* tmp = zjn_string_concat(result, zjn_val_string(sep));
                zjn_val_free(result);
                result = tmp;
            }
            ZjnValue* tmp = zjn_string_concat(result, lst->items[i]);
            zjn_val_free(result);
            result = tmp;
        }
    }
    (void)seplen;
    return result;
}

/* ---------- replace ---------- */
ZjnValue* zjn_builtin_replace(int argc, ZjnValue** args) {
    (void)argc;
    if (!args[0] || args[0]->type != ZVAL_STRING) return zjn_val_string("");
    const char* s = args[0]->data.string_val;
    const char* old = zjn_val_to_string(args[1]);
    const char* news = zjn_val_to_string(args[2]);
    size_t olen = strlen(old);
    if (olen == 0) return zjn_val_copy(args[0]);

    /* 计算所需缓冲大小 */
    size_t capacity = strlen(s) + 1;
    const char* p = s;
    while ((p = strstr(p, old)) != NULL) {
        capacity += strlen(news) - olen;
        p += olen;
    }
    char* buf = (char*)malloc(capacity);
    if (!buf) return zjn_val_string("");
    char* out = buf;
    p = s;
    while (1) {
        const char* hit = strstr(p, old);
        if (!hit) {
            strcpy(out, p);
            break;
        }
        size_t lead = (size_t)(hit - p);
        memcpy(out, p, lead);
        out += lead;
        strcpy(out, news);
        out += strlen(news);
        p = hit + olen;
    }
    ZjnValue* v = zjn_val_string(buf);
    free(buf);
    return v;
}

/* ---------- find ---------- */
ZjnValue* zjn_builtin_find(int argc, ZjnValue** args) {
    if (argc == 0 || !args[0] || args[0]->type != ZVAL_STRING)
        return zjn_val_int(-1);
    const char* s = args[0]->data.string_val;
    const char* sub = zjn_val_to_string(args[1]);
    long start = 0;
    if (argc >= 3) start = zjn_val_to_int(args[2]);
    if (start < 0) start = 0;
    if (start >= (long)strlen(s)) return zjn_val_int(-1);
    const char* p = strstr(s + start, sub);
    if (p) return zjn_val_int((long)(p - s));
    return zjn_val_int(-1);
}

/* ---------- strip 系列 ---------- */
static ZjnValue* strip_impl(const char* s, int left, int right) {
    size_t len = strlen(s);
    size_t start = 0, end = len;
    if (left)
        while (start < len && isspace((unsigned char)s[start])) start++;
    if (right)
        while (end > start && isspace((unsigned char)s[end - 1])) end--;
    return zjn_val_string_from(s + start, (int)(end - start));
}

ZjnValue* zjn_builtin_strip(int argc, ZjnValue** args) {
    (void)argc;
    return strip_impl(zjn_val_to_string(args[0]), 1, 1);
}

ZjnValue* zjn_builtin_lstrip(int argc, ZjnValue** args) {
    (void)argc;
    return strip_impl(zjn_val_to_string(args[0]), 1, 0);
}

ZjnValue* zjn_builtin_rstrip(int argc, ZjnValue** args) {
    (void)argc;
    return strip_impl(zjn_val_to_string(args[0]), 0, 1);
}

/* ---------- upper / lower ---------- */
ZjnValue* zjn_builtin_upper(int argc, ZjnValue** args) {
    (void)argc;
    const char* s = zjn_val_to_string(args[0]);
    size_t len = strlen(s);
    char* buf = (char*)malloc(len + 1);
    if (!buf) return zjn_val_string("");
    for (size_t i = 0; i < len; i++)
        buf[i] = (char)toupper((unsigned char)s[i]);
    buf[len] = '\0';
    ZjnValue* v = zjn_val_string(buf);
    free(buf);
    return v;
}

ZjnValue* zjn_builtin_lower(int argc, ZjnValue** args) {
    (void)argc;
    const char* s = zjn_val_to_string(args[0]);
    size_t len = strlen(s);
    char* buf = (char*)malloc(len + 1);
    if (!buf) return zjn_val_string("");
    for (size_t i = 0; i < len; i++)
        buf[i] = (char)tolower((unsigned char)s[i]);
    buf[len] = '\0';
    ZjnValue* v = zjn_val_string(buf);
    free(buf);
    return v;
}

/* ---------- startswith / endswith ---------- */
ZjnValue* zjn_builtin_startswith(int argc, ZjnValue** args) {
    (void)argc;
    const char* s = zjn_val_to_string(args[0]);
    const char* prefix = zjn_val_to_string(args[1]);
    size_t plen = strlen(prefix);
    return zjn_val_bool(strlen(s) >= plen && strncmp(s, prefix, plen) == 0);
}

ZjnValue* zjn_builtin_endswith(int argc, ZjnValue** args) {
    (void)argc;
    const char* s = zjn_val_to_string(args[0]);
    const char* suffix = zjn_val_to_string(args[1]);
    size_t slen = strlen(s), suflen = strlen(suffix);
    return zjn_val_bool(slen >= suflen &&
                        strcmp(s + slen - suflen, suffix) == 0);
}

/* ===================================================================
 *  迭代器组合
 * =================================================================== */

/* ---------- enumerate：返回 [索引, 元素] 对列表 ---------- */
ZjnValue* zjn_builtin_enumerate(int argc, ZjnValue** args) {
    (void)argc;
    ZjnValue* result = zjn_val_list();
    if (!args[0]) return result;
    if (ZJN_IS_LIST(args[0])) {
        ZjnList* lst = args[0]->data.list_val;
        for (int i = 0; i < lst->count; i++) {
            ZjnValue* pair = zjn_val_list();
            zjn_list_append(pair, zjn_val_int(i));
            zjn_list_append(pair, zjn_val_copy(lst->items[i]));
            zjn_list_append(result, pair);
        }
    } else if (ZJN_IS_STRING(args[0])) {
        const char* s = args[0]->data.string_val;
        for (int i = 0; s[i] != '\0'; i++) {
            char ch[2] = { s[i], '\0' };
            ZjnValue* pair = zjn_val_list();
            zjn_list_append(pair, zjn_val_int(i));
            zjn_list_append(pair, zjn_val_string(ch));
            zjn_list_append(result, pair);
        }
    } else if (ZJN_IS_DICT(args[0])) {
        ZjnDict* d = args[0]->data.dict_val;
        for (int i = 0; i < d->count; i++) {
            ZjnValue* pair = zjn_val_list();
            zjn_list_append(pair, zjn_val_int(i));
            zjn_list_append(pair, zjn_val_string(d->entries[i].key));
            zjn_list_append(result, pair);
        }
    }
    return result;
}

/* ---------- zip：两个序列配对 ---------- */
ZjnValue* zjn_builtin_zip(int argc, ZjnValue** args) {
    (void)argc;
    ZjnValue* result = zjn_val_list();
    if (!args[0] || !args[1]) return result;
    int n = 0;
    if (ZJN_IS_LIST(args[0])) n = args[0]->data.list_val->count;
    else if (ZJN_IS_STRING(args[0])) n = (int)strlen(args[0]->data.string_val);
    int m = 0;
    if (ZJN_IS_LIST(args[1])) m = args[1]->data.list_val->count;
    else if (ZJN_IS_STRING(args[1])) m = (int)strlen(args[1]->data.string_val);
    int len = n < m ? n : m;
    for (int i = 0; i < len; i++) {
        ZjnValue* pair = zjn_val_list();
        ZjnValue* a = zjn_val_index(args[0], i);
        ZjnValue* b = zjn_val_index(args[1], i);
        zjn_list_append(pair, a ? a : zjn_val_none());
        zjn_list_append(pair, b ? b : zjn_val_none());
        zjn_list_append(result, pair);
    }
    return result;
}

/* ===================================================================
 *  字典
 * =================================================================== */

/* ---------- keys ---------- */
ZjnValue* zjn_builtin_keys(int argc, ZjnValue** args) {
    (void)argc;
    ZjnValue* result = zjn_val_list();
    if (!args[0] || args[0]->type != ZVAL_DICT) return result;
    ZjnDict* d = args[0]->data.dict_val;
    for (int i = 0; i < d->count; i++)
        zjn_list_append(result, zjn_val_string(d->entries[i].key));
    return result;
}

/* ---------- values ---------- */
ZjnValue* zjn_builtin_values(int argc, ZjnValue** args) {
    (void)argc;
    ZjnValue* result = zjn_val_list();
    if (!args[0] || args[0]->type != ZVAL_DICT) return result;
    ZjnDict* d = args[0]->data.dict_val;
    for (int i = 0; i < d->count; i++)
        zjn_list_append(result, zjn_val_copy(d->entries[i].value));
    return result;
}

#ifdef _WIN32
/* ===================================================================
 *  GUI：窗口程序支持（Win32 原生实现，独立设计）
 *  事件模型：tkinter 风格轮询。
 *
 *  用法示例：
 *    win = window('我的窗口', 400, 300)
 *    btn = button(win, '点我', 20, 60, 80, 30)
 *    show(win)
 *    while is_open(win):
 *        cid = wait_click(win)
 *        if cid == 0: break            # 窗口被关闭
 *        if cid == btn: put('被点击')
 * =================================================================== */

#define ZJN_GUI_MAX_WND   32
#define ZJN_GUI_MAX_CTRL  512
#define ZJN_GUI_MAX_QUEUE 128

/* 窗口表（id = 下标 + 1） */
static HWND  g_gui_wnds[ZJN_GUI_MAX_WND];
static int   g_gui_wnd_closed[ZJN_GUI_MAX_WND];
static int   g_gui_wnd_count = 0;

/* 控件表（id = 下标 + 1，全局递增） */
static HWND  g_gui_ctrls[ZJN_GUI_MAX_CTRL];
static int   g_gui_ctrl_count = 0;

/* 按钮点击队列 */
static int   g_gui_clicks[ZJN_GUI_MAX_QUEUE];
static int   g_gui_click_head = 0, g_gui_click_tail = 0;

static const wchar_t* g_gui_class = L"ZjnWindow";

/* ---------- 编码转换（UTF-8 <-> UTF-16） ---------- */
static wchar_t* gui_utf8_to_w(const char* utf8) {
    if (!utf8) utf8 = "";
    int len = MultiByteToWideChar(CP_UTF8, 0, utf8, -1, NULL, 0);
    wchar_t* buf = (wchar_t*)malloc((len + 1) * sizeof(wchar_t));
    if (!buf) return NULL;
    MultiByteToWideChar(CP_UTF8, 0, utf8, -1, buf, len);
    return buf;
}

static char* gui_w_to_utf8(const wchar_t* w) {
    int len = WideCharToMultiByte(CP_UTF8, 0, w, -1, NULL, 0, NULL, NULL);
    char* buf = (char*)malloc(len + 1);
    if (!buf) return NULL;
    WideCharToMultiByte(CP_UTF8, 0, w, -1, buf, len, NULL, NULL);
    return buf;
}

/* ---------- HWND -> 窗口 id ---------- */
static int gui_find_wnd(HWND hwnd) {
    for (int i = 0; i < g_gui_wnd_count; i++) {
        if (g_gui_wnds[i] == hwnd) return i + 1;
    }
    return 0;
}

/* ---------- 窗口过程 ---------- */
static LRESULT CALLBACK gui_wnd_proc(HWND hwnd, UINT msg, WPARAM w, LPARAM l) {
    switch (msg) {
        case WM_COMMAND:
            /* 按钮点击：LOWORD(wParam) 为控件 id */
            if (HIWORD(w) == BN_CLICKED) {
                int cid = (int)(INT_PTR)LOWORD(w);
                if (g_gui_click_tail - g_gui_click_head < ZJN_GUI_MAX_QUEUE) {
                    g_gui_clicks[g_gui_click_tail % ZJN_GUI_MAX_QUEUE] = cid;
                    g_gui_click_tail++;
                }
            }
            return 0;
        case WM_CLOSE:
            DestroyWindow(hwnd);
            return 0;
        case WM_DESTROY: {
            int wid = gui_find_wnd(hwnd);
            if (wid > 0) g_gui_wnd_closed[wid - 1] = 1;
            return 0;
        }
    }
    return DefWindowProcW(hwnd, msg, w, l);
}

/* ---------- 注册窗口类（只注册一次） ---------- */
static int gui_ensure_class(void) {
    static int registered = 0;
    if (registered) return 1;
    WNDCLASSW wc;
    memset(&wc, 0, sizeof(wc));
    wc.lpfnWndProc = gui_wnd_proc;
    wc.hInstance = GetModuleHandleW(NULL);
    wc.lpszClassName = g_gui_class;
    wc.hbrBackground = (HBRUSH)(COLOR_WINDOW + 1);
    wc.hCursor = LoadCursorW(NULL, (LPCWSTR)IDC_ARROW);
    if (!RegisterClassW(&wc)) return 0;
    registered = 1;
    return 1;
}

/* ---------- 创建子控件通用入口 ---------- */
static ZjnValue* gui_create_ctrl(int wid, const wchar_t* cls, const wchar_t* text,
                                 DWORD style, int x, int y, int w, int h) {
    if (wid < 1 || wid > g_gui_wnd_count) return zjn_val_int(0);
    if (g_gui_ctrl_count >= ZJN_GUI_MAX_CTRL) return zjn_val_int(0);

    int cid = ++g_gui_ctrl_count;
    HWND ctrl = CreateWindowExW(0, cls, text ? text : L"",
                                WS_CHILD | WS_VISIBLE | style,
                                x, y, w, h,
                                g_gui_wnds[wid - 1], (HMENU)(INT_PTR)cid,
                                GetModuleHandleW(NULL), NULL);
    if (!ctrl) return zjn_val_int(0);
    g_gui_ctrls[cid - 1] = ctrl;
    return zjn_val_int(cid);
}

/* ===================================================================
 *  内置函数实现
 * =================================================================== */

/* sleep(毫秒)：暂停指定时间 */
ZjnValue* zjn_builtin_sleep(int argc, ZjnValue** args) {
    if (argc < 1) return zjn_val_none();
    long ms = zjn_val_to_int(args[0]);
    if (ms < 0) ms = 0;
    if (ms > 3600000) ms = 3600000; /* 上限 1 小时，防误写 */
    Sleep((DWORD)ms);
    return zjn_val_none();
}

/* window(标题, 宽, 高) -> 窗口 id */
ZjnValue* zjn_builtin_window(int argc, ZjnValue** args) {
    if (argc < 3) return zjn_val_int(0);
    if (!gui_ensure_class()) return zjn_val_int(0);
    if (g_gui_wnd_count >= ZJN_GUI_MAX_WND) return zjn_val_int(0);

    wchar_t* title = gui_utf8_to_w(zjn_val_to_string(args[0]));
    if (!title) return zjn_val_int(0);
    int w = (int)zjn_val_to_int(args[1]);
    int h = (int)zjn_val_to_int(args[2]);
    if (w < 100) w = 100;
    if (h < 100) h = 100;

    HWND hwnd = CreateWindowExW(0, g_gui_class, title,
                                WS_OVERLAPPEDWINDOW,
                                CW_USEDEFAULT, CW_USEDEFAULT, w, h,
                                NULL, NULL, GetModuleHandleW(NULL), NULL);
    free(title);
    if (!hwnd) return zjn_val_int(0);

    int id = ++g_gui_wnd_count;
    g_gui_wnds[id - 1] = hwnd;
    g_gui_wnd_closed[id - 1] = 0;
    return zjn_val_int(id);
}

/* label(窗口, 文本, x, y) -> 控件 id */
ZjnValue* zjn_builtin_label(int argc, ZjnValue** args) {
    if (argc < 4) return zjn_val_int(0);
    wchar_t* text = gui_utf8_to_w(zjn_val_to_string(args[1]));
    if (!text) return zjn_val_int(0);
    ZjnValue* r = gui_create_ctrl((int)zjn_val_to_int(args[0]), L"STATIC", text,
                                  SS_LEFT, (int)zjn_val_to_int(args[2]),
                                  (int)zjn_val_to_int(args[3]), 200, 24);
    free(text);
    return r;
}

/* button(窗口, 文本, x, y, 宽, 高) -> 控件 id */
ZjnValue* zjn_builtin_button(int argc, ZjnValue** args) {
    if (argc < 6) return zjn_val_int(0);
    wchar_t* text = gui_utf8_to_w(zjn_val_to_string(args[1]));
    if (!text) return zjn_val_int(0);
    int w = (int)zjn_val_to_int(args[4]);
    int h = (int)zjn_val_to_int(args[5]);
    if (w < 40) w = 40;
    if (h < 20) h = 20;
    ZjnValue* r = gui_create_ctrl((int)zjn_val_to_int(args[0]), L"BUTTON", text,
                                  BS_PUSHBUTTON, (int)zjn_val_to_int(args[2]),
                                  (int)zjn_val_to_int(args[3]), w, h);
    free(text);
    return r;
}

/* textbox(窗口, x, y, 宽, 高) -> 控件 id */
ZjnValue* zjn_builtin_textbox(int argc, ZjnValue** args) {
    if (argc < 5) return zjn_val_int(0);
    int w = (int)zjn_val_to_int(args[3]);
    int h = (int)zjn_val_to_int(args[4]);
    if (w < 40) w = 40;
    if (h < 20) h = 20;
    return gui_create_ctrl((int)zjn_val_to_int(args[0]), L"EDIT", L"",
                           ES_AUTOHSCROLL | ES_LEFT, (int)zjn_val_to_int(args[1]),
                           (int)zjn_val_to_int(args[2]), w, h);
}

/* show(窗口)：显示窗口 */
ZjnValue* zjn_builtin_show(int argc, ZjnValue** args) {
    if (argc < 1) return zjn_val_none();
    int wid = (int)zjn_val_to_int(args[0]);
    if (wid >= 1 && wid <= g_gui_wnd_count) {
        ShowWindow(g_gui_wnds[wid - 1], SW_SHOW);
        UpdateWindow(g_gui_wnds[wid - 1]);
    }
    return zjn_val_none();
}

/* wait_click(窗口)：等待按钮点击，返回控件 id；窗口关闭返回 0 */
ZjnValue* zjn_builtin_wait_click(int argc, ZjnValue** args) {
    int wid = argc >= 1 ? (int)zjn_val_to_int(args[0]) : 0;

    while (g_gui_click_head == g_gui_click_tail) {
        if (wid >= 1 && wid <= g_gui_wnd_count && g_gui_wnd_closed[wid - 1]) {
            return zjn_val_int(0);
        }
        MSG msg;
        while (PeekMessageW(&msg, NULL, 0, 0, PM_REMOVE)) {
            if (msg.message == WM_QUIT) return zjn_val_int(0);
            TranslateMessage(&msg);
            DispatchMessageW(&msg);
        }
        Sleep(10); /* 防忙等 */
    }
    int cid = g_gui_clicks[g_gui_click_head % ZJN_GUI_MAX_QUEUE];
    g_gui_click_head++;
    return zjn_val_int(cid);
}

/* is_open(窗口)：窗口是否打开 */
ZjnValue* zjn_builtin_is_open(int argc, ZjnValue** args) {
    if (argc < 1) return zjn_val_bool(0);
    int wid = (int)zjn_val_to_int(args[0]);
    if (wid < 1 || wid > g_gui_wnd_count) return zjn_val_bool(0);
    return zjn_val_bool(!g_gui_wnd_closed[wid - 1]);
}

/* close(窗口)：关闭窗口 */
ZjnValue* zjn_builtin_close(int argc, ZjnValue** args) {
    if (argc < 1) return zjn_val_none();
    int wid = (int)zjn_val_to_int(args[0]);
    if (wid >= 1 && wid <= g_gui_wnd_count) {
        DestroyWindow(g_gui_wnds[wid - 1]);
        g_gui_wnd_closed[wid - 1] = 1;
    }
    return zjn_val_none();
}

/* get_text(控件)：读取文本 */
ZjnValue* zjn_builtin_get_text(int argc, ZjnValue** args) {
    if (argc < 1) return zjn_val_string("");
    int cid = (int)zjn_val_to_int(args[0]);
    if (cid < 1 || cid > g_gui_ctrl_count) return zjn_val_string("");
    wchar_t buf[1024];
    int n = GetWindowTextW(g_gui_ctrls[cid - 1], buf, 1023);
    if (n < 0) n = 0;
    buf[n] = L'\0';
    char* utf8 = gui_w_to_utf8(buf);
    if (!utf8) return zjn_val_string("");
    ZjnValue* v = zjn_val_string(utf8);
    free(utf8);
    return v;
}

/* set_text(控件, 文本)：设置文本 */
ZjnValue* zjn_builtin_set_text(int argc, ZjnValue** args) {
    if (argc < 2) return zjn_val_none();
    int cid = (int)zjn_val_to_int(args[0]);
    if (cid < 1 || cid > g_gui_ctrl_count) return zjn_val_none();
    wchar_t* text = gui_utf8_to_w(zjn_val_to_string(args[1]));
    if (!text) return zjn_val_none();
    SetWindowTextW(g_gui_ctrls[cid - 1], text);
    free(text);
    return zjn_val_none();
}

/* msgbox(标题, 内容)：消息框 */
ZjnValue* zjn_builtin_msgbox(int argc, ZjnValue** args) {
    wchar_t* title = gui_utf8_to_w(argc >= 1 ? zjn_val_to_string(args[0]) : "");
    wchar_t* text = gui_utf8_to_w(argc >= 2 ? zjn_val_to_string(args[1]) : "");
    if (title && text) {
        MessageBoxW(NULL, text, title, MB_OK | MB_ICONINFORMATION);
    }
    if (title) free(title);
    if (text) free(text);
    return zjn_val_none();
}
#endif /* _WIN32 */
