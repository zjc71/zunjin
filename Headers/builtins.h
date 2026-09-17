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
 * zunjin 语言 - 内置函数接口
 * 注册全局可用的内置函数
 * 完全独立重新实现
 *
 * 版本 2.0 变更：
 *  - print 等输出类内置经输出模块（zjn_builtins_set_output）输出，
 *    保证 IDE 终端与脚本输出一致。
 *  - 新增数学、字符串、字典、迭代器组合等内置函数（对照主流脚本语言标准）。
 * ========================================================================== */

#ifndef ZUNJIN_BUILTINS_H
#define ZUNJIN_BUILTINS_H

#include "zunjin.h"
#include "object.h"
#include "env.h"
#include "output.h"

/* ---------- 输出模块绑定（print/read 提示符输出用） ---------- */
void zjn_builtins_set_output(ZjnOutput* output);

/* ---------- 注册所有内置函数到环境 ---------- */
void zjn_builtins_register_all(ZjnEnv* env);

/* ---------- 各内置函数声明 ---------- */
/* 基础 I/O */
ZjnValue* zjn_builtin_print(int argc, ZjnValue** args);
ZjnValue* zjn_builtin_input(int argc, ZjnValue** args);

/* 序列 */
ZjnValue* zjn_builtin_range(int argc, ZjnValue** args);
ZjnValue* zjn_builtin_len(int argc, ZjnValue** args);
ZjnValue* zjn_builtin_list(int argc, ZjnValue** args);
ZjnValue* zjn_builtin_append(int argc, ZjnValue** args);
ZjnValue* zjn_builtin_pop(int argc, ZjnValue** args);
ZjnValue* zjn_builtin_insert(int argc, ZjnValue** args);
ZjnValue* zjn_builtin_remove(int argc, ZjnValue** args);
ZjnValue* zjn_builtin_delete_at(int argc, ZjnValue** args);
ZjnValue* zjn_builtin_clear(int argc, ZjnValue** args);
ZjnValue* zjn_builtin_extend(int argc, ZjnValue** args);
ZjnValue* zjn_builtin_count(int argc, ZjnValue** args);
ZjnValue* zjn_builtin_index_of(int argc, ZjnValue** args);

/* 类型转换 */
ZjnValue* zjn_builtin_str(int argc, ZjnValue** args);
ZjnValue* zjn_builtin_int(int argc, ZjnValue** args);
ZjnValue* zjn_builtin_float(int argc, ZjnValue** args);
ZjnValue* zjn_builtin_bool(int argc, ZjnValue** args);
ZjnValue* zjn_builtin_type(int argc, ZjnValue** args);
ZjnValue* zjn_builtin_ord(int argc, ZjnValue** args);
ZjnValue* zjn_builtin_chr(int argc, ZjnValue** args);
ZjnValue* zjn_builtin_hex(int argc, ZjnValue** args);
ZjnValue* zjn_builtin_oct(int argc, ZjnValue** args);
ZjnValue* zjn_builtin_bin(int argc, ZjnValue** args);

/* 数值运算 */
ZjnValue* zjn_builtin_min(int argc, ZjnValue** args);
ZjnValue* zjn_builtin_max(int argc, ZjnValue** args);
ZjnValue* zjn_builtin_sum(int argc, ZjnValue** args);
ZjnValue* zjn_builtin_abs(int argc, ZjnValue** args);
ZjnValue* zjn_builtin_round(int argc, ZjnValue** args);
ZjnValue* zjn_builtin_pow(int argc, ZjnValue** args);
ZjnValue* zjn_builtin_sqrt(int argc, ZjnValue** args);
ZjnValue* zjn_builtin_floor(int argc, ZjnValue** args);
ZjnValue* zjn_builtin_ceil(int argc, ZjnValue** args);

/* 排序 */
ZjnValue* zjn_builtin_sorted(int argc, ZjnValue** args);
ZjnValue* zjn_builtin_reversed(int argc, ZjnValue** args);

/* 字符串处理 */
ZjnValue* zjn_builtin_split(int argc, ZjnValue** args);
ZjnValue* zjn_builtin_join(int argc, ZjnValue** args);
ZjnValue* zjn_builtin_replace(int argc, ZjnValue** args);
ZjnValue* zjn_builtin_find(int argc, ZjnValue** args);
ZjnValue* zjn_builtin_strip(int argc, ZjnValue** args);
ZjnValue* zjn_builtin_lstrip(int argc, ZjnValue** args);
ZjnValue* zjn_builtin_rstrip(int argc, ZjnValue** args);
ZjnValue* zjn_builtin_upper(int argc, ZjnValue** args);
ZjnValue* zjn_builtin_lower(int argc, ZjnValue** args);
ZjnValue* zjn_builtin_startswith(int argc, ZjnValue** args);
ZjnValue* zjn_builtin_endswith(int argc, ZjnValue** args);

/* 迭代器组合 */
ZjnValue* zjn_builtin_enumerate(int argc, ZjnValue** args);
ZjnValue* zjn_builtin_zip(int argc, ZjnValue** args);

/* 字典 */
ZjnValue* zjn_builtin_keys(int argc, ZjnValue** args);
ZjnValue* zjn_builtin_values(int argc, ZjnValue** args);

#ifdef _WIN32
/* ---------- GUI（窗口程序支持，仅 Windows） ---------- */
ZjnValue* zjn_builtin_sleep(int argc, ZjnValue** args);     /* sleep(毫秒)：暂停 */
ZjnValue* zjn_builtin_window(int argc, ZjnValue** args);   /* window(标题, 宽, 高) → 窗口 id */
ZjnValue* zjn_builtin_label(int argc, ZjnValue** args);    /* label(窗口, 文本, x, y) → 控件 id */
ZjnValue* zjn_builtin_button(int argc, ZjnValue** args);   /* button(窗口, 文本, x, y, w, h) → 控件 id */
ZjnValue* zjn_builtin_textbox(int argc, ZjnValue** args);  /* textbox(窗口, x, y, w, h) → 控件 id */
ZjnValue* zjn_builtin_show(int argc, ZjnValue** args);     /* show(窗口)：显示窗口 */
ZjnValue* zjn_builtin_wait_click(int argc, ZjnValue** args); /* wait_click(窗口)：等待按钮点击，返回控件 id；窗口关闭返回 0 */
ZjnValue* zjn_builtin_is_open(int argc, ZjnValue** args);  /* is_open(窗口)：窗口是否打开 */
ZjnValue* zjn_builtin_close(int argc, ZjnValue** args);    /* close(窗口)：关闭窗口 */
ZjnValue* zjn_builtin_get_text(int argc, ZjnValue** args); /* get_text(控件)：读取文本 */
ZjnValue* zjn_builtin_set_text(int argc, ZjnValue** args); /* set_text(控件, 文本)：设置文本 */
ZjnValue* zjn_builtin_msgbox(int argc, ZjnValue** args);   /* msgbox(标题, 内容)：消息框 */
#endif /* _WIN32 */

#endif /* ZUNJIN_BUILTINS_H */
