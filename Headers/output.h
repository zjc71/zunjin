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
 * zunjin 语言 - 输出模块接口
 * 与运算逻辑完全解耦，独立负责格式化与显示输出
 * ========================================================================== */

#ifndef ZUNJIN_OUTPUT_H
#define ZUNJIN_OUTPUT_H

#include "zunjin.h"
#include "object.h"

/* ---------- 输出模块 ---------- */
struct ZjnOutput {
    FILE* stream;
    char** buffer;
    int buffer_count;
    int buffer_capacity;
};

/* ---------- 函数 ---------- */
ZjnOutput* zjn_output_create(FILE* stream);
void zjn_output_free(ZjnOutput* out);

/* 核心输出 */
void zjn_output_write(ZjnOutput* out, const char* text);
void zjn_output_writeln(ZjnOutput* out, const char* text);
void zjn_output_error(ZjnOutput* out, const char* fmt, ...);

/* 值格式化输出 */
void zjn_output_print_value(ZjnOutput* out, ZjnValue** values, int count);
void zjn_output_print_result(ZjnOutput* out, ZjnValue* value);

/* 格式化辅助 */
const char* zjn_output_format_value(ZjnOutput* out, const ZjnValue* val);

/* 缓冲区操作 */
const char* zjn_output_get_buffer(ZjnOutput* out);
void zjn_output_clear_buffer(ZjnOutput* out);
void zjn_output_flush(ZjnOutput* out);

#endif /* ZUNJIN_OUTPUT_H */