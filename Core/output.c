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
 * zunjin 语言 - 输出模块实现
 * 与运算逻辑完全解耦，独立负责格式化与显示输出
 * 完全独立重新实现
 * ========================================================================== */

#include "output.h"
#include <string.h>
#include <stdio.h>
#include <stdarg.h>

/* ---------- 创建输出模块 ---------- */
ZjnOutput* zjn_output_create(FILE* stream) {
    ZjnOutput* out = ZUNJIN_ALLOC(ZjnOutput);
    if (!out) return NULL;
    out->stream = stream ? stream : stdout;
    out->buffer = NULL;
    out->buffer_count = 0;
    out->buffer_capacity = 0;
    return out;
}

/* ---------- 释放输出模块 ---------- */
void zjn_output_free(ZjnOutput* out) {
    if (!out) return;
    if (out->buffer) {
        for (int i = 0; i < out->buffer_count; i++)
            if (out->buffer[i]) free(out->buffer[i]);
        free(out->buffer);
    }
    free(out);
}

/* ---------- 核心输出 ---------- */
void zjn_output_write(ZjnOutput* out, const char* text) {
    if (out->stream) {
        fprintf(out->stream, "%s", text);
    }
}

void zjn_output_writeln(ZjnOutput* out, const char* text) {
    if (out->stream) {
        fprintf(out->stream, "%s\n", text);
    }
}

void zjn_output_error(ZjnOutput* out, const char* fmt, ...) {
    va_list args;
    va_start(args, fmt);
    if (out && out->stream) {
        vfprintf(out->stream, fmt, args);
    } else {
        vfprintf(stderr, fmt, args);
    }
    fprintf(out && out->stream ? out->stream : stderr, "\n");
    va_end(args);
}

/* ---------- 值格式化辅助（递归，避免静态缓冲区冲突） ---------- */
static void format_value_to_buf(const ZjnValue* val, char* buf, int size) {
    if (!val || size <= 0) {
        if (size > 0) buf[0] = '\0';
        return;
    }

    switch (val->type) {
        case ZVAL_NONE:
            strncpy(buf, "null", size - 1);
            buf[size - 1] = '\0';
            break;
        case ZVAL_BOOL:
            strncpy(buf, val->data.bool_val ? "yes" : "no", size - 1);
            buf[size - 1] = '\0';
            break;
        case ZVAL_INT:
            snprintf(buf, size, "%ld", val->data.int_val);
            break;
        case ZVAL_FLOAT: {
            double d = val->data.float_val;
            if (d == (long)d) {
                snprintf(buf, size, "%.1f", d);
            } else {
                snprintf(buf, size, "%.10g", d);
            }
            break;
        }
        case ZVAL_STRING:
            strncpy(buf, val->data.string_val, size - 1);
            buf[size - 1] = '\0';
            break;
        case ZVAL_LIST: {
            int pos = 0;
            buf[pos++] = '[';
            ZjnList* lst = val->data.list_val;
            for (int i = 0; i < lst->count && pos < size - 10; i++) {
                if (i > 0) {
                    buf[pos++] = ','; buf[pos++] = ' ';
                }
                char item_buf[512];
                format_value_to_buf(lst->items[i], item_buf, sizeof(item_buf));
                int len = (int)strlen(item_buf);
                if (pos + len < size - 5) {
                    memcpy(buf + pos, item_buf, len);
                    pos += len;
                }
            }
            buf[pos++] = ']';
            buf[pos] = '\0';
            break;
        }
        case ZVAL_DICT: {
            int pos = 0;
            buf[pos++] = '{';
            ZjnDict* d = val->data.dict_val;
            for (int i = 0; i < d->count && pos < size - 10; i++) {
                if (i > 0) {
                    buf[pos++] = ','; buf[pos++] = ' ';
                }
                int klen = (int)strlen(d->entries[i].key);
                if (pos + klen + 8 < size) {
                    buf[pos++] = '\'';
                    memcpy(buf + pos, d->entries[i].key, klen);
                    pos += klen;
                    buf[pos++] = '\'';
                    buf[pos++] = ':'; buf[pos++] = ' ';
                }
                char item_buf[512];
                format_value_to_buf(d->entries[i].value, item_buf, sizeof(item_buf));
                int len = (int)strlen(item_buf);
                if (pos + len < size - 5) {
                    memcpy(buf + pos, item_buf, len);
                    pos += len;
                }
            }
            buf[pos++] = '}';
            buf[pos] = '\0';
            break;
        }
        case ZVAL_FUNC:
            snprintf(buf, size, "<函数 %s>", val->data.func_val->name);
            break;
        case ZVAL_BUILTIN:
            snprintf(buf, size, "<内置函数 %s>", val->data.builtin_val->name);
            break;
        case ZVAL_MODULE:
            snprintf(buf, size, "<模块 %s>", val->data.module_val->name);
            break;
        default:
            strncpy(buf, "<未知>", size - 1);
            buf[size - 1] = '\0';
            break;
    }
}

/* ---------- 值格式化 ---------- */
const char* zjn_output_format_value(ZjnOutput* out, const ZjnValue* val) {
    (void)out;
    static char buffer[2048];
    format_value_to_buf(val, buffer, sizeof(buffer));
    return buffer;
}

/* ---------- 打印值 ---------- */
void zjn_output_print_value(ZjnOutput* out, ZjnValue** values, int count) {
    char line[4096] = {0};
    int pos = 0;

    for (int i = 0; i < count && pos < (int)sizeof(line) - 100; i++) {
        if (i > 0) {
            line[pos++] = ' ';
            line[pos] = '\0';
        }
        const char* formatted = zjn_output_format_value(out, values[i]);
        int len = (int)strlen(formatted);
        if (pos + len < (int)sizeof(line) - 10) {
            memcpy(line + pos, formatted, len);
            pos += len;
        }
    }

    zjn_output_writeln(out, line);

    /* 存入缓冲区 */
    if (out->buffer_count >= out->buffer_capacity) {
        int new_cap = out->buffer_capacity == 0 ? 16 : out->buffer_capacity * 2;
        char** new_buf = realloc(out->buffer, new_cap * sizeof(char*));
        if (new_buf) {
            out->buffer = new_buf;
            out->buffer_capacity = new_cap;
        }
    }
    if (out->buffer_count < out->buffer_capacity) {
        out->buffer[out->buffer_count++] = strdup(line);
    }
}

void zjn_output_print_result(ZjnOutput* out, ZjnValue* value) {
    const char* formatted = zjn_output_format_value(out, value);
    zjn_output_writeln(out, formatted);
}

/* ---------- 缓冲区操作 ---------- */
const char* zjn_output_get_buffer(ZjnOutput* out) {
    if (out->buffer_count > 0) {
        return out->buffer[out->buffer_count - 1];
    }
    return "";
}

void zjn_output_clear_buffer(ZjnOutput* out) {
    for (int i = 0; i < out->buffer_count; i++)
        if (out->buffer[i]) free(out->buffer[i]);
    out->buffer_count = 0;
}

void zjn_output_flush(ZjnOutput* out) {
    if (out->stream) {
        fflush(out->stream);
    }
}