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
 * zunjin 编程语言 - 主头文件
 * zunjin 编程语言主头文件，完全独立实现
 * 所有类型定义、常量、宏在此统一导出
 * ========================================================================== */

#ifndef ZUNJIN_H
#define ZUNJIN_H

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdint.h>
#include <stdbool.h>
#include <stdarg.h>
#include <ctype.h>
#include <setjmp.h>

/* ---------- 版本信息 ---------- */
#define ZUNJIN_VERSION "0.0.1"
#define ZUNJIN_CODENAME "zunjin"

/* ---------- 平台检测 ---------- */
#ifdef _WIN32
    #define ZUNJIN_OS_WIN
#elif defined(__linux__)
    #define ZUNJIN_OS_LINUX
#elif defined(__APPLE__)
    #define ZUNJIN_OS_MAC
#endif

/* ---------- 导出符号 ---------- */
#ifdef ZUNJIN_BUILD_LIB
    #ifdef _WIN32
        #define ZUNJIN_API __declspec(dllexport)
    #else
        #define ZUNJIN_API __attribute__((visibility("default")))
    #endif
#else
    #define ZUNJIN_API
#endif

/* ---------- 基础常量 ---------- */
#define ZUNJIN_MAX_LINE 1024
#define ZUNJIN_MAX_TOKENS 65536
#define ZUNJIN_MAX_NESTING 256
#define ZUNJIN_MAX_SOURCE 1048576

/* ---------- 内存分配宏 ---------- */
#define ZUNJIN_ALLOC(type)       ((type*)malloc(sizeof(type)))
#define ZUNJIN_ALLOC_N(type, n)  ((type*)malloc((n) * sizeof(type)))
#define ZUNJIN_REALLOC(p, n)     ((void*)realloc((p), (n)))
#define ZUNJIN_FREE(p)           do { if (p) { free(p); (p) = NULL; } } while(0)

/* ---------- 前向声明 ---------- */
typedef struct ZjnValue      ZjnValue;
typedef struct ZjnToken      ZjnToken;
typedef struct ZjnLexer      ZjnLexer;
typedef struct ZjnAstNode    ZjnAstNode;
typedef struct ZjnParser     ZjnParser;
typedef struct ZjnEnv        ZjnEnv;
typedef struct ZjnOutput     ZjnOutput;
typedef struct ZjnVM         ZjnVM;
typedef struct ZjnFunc       ZjnFunc;
typedef struct ZjnBuiltin    ZjnBuiltin;

/* ---------- 错误处理 ---------- */
typedef struct {
    char message[512];
    int line;
    int column;
    int has_error;
} ZjnError;

/* 全局错误跳转 */
extern jmp_buf zjn_error_buf;
extern ZjnError zjn_last_error;

#define ZJN_THROW(msg, ln, col) do { \
    snprintf(zjn_last_error.message, sizeof(zjn_last_error.message), "%s", (msg)); \
    zjn_last_error.line = (ln); \
    zjn_last_error.column = (col); \
    zjn_last_error.has_error = 1; \
    longjmp(zjn_error_buf, 1); \
} while(0)

#define ZJN_TRY()    if (setjmp(zjn_error_buf) == 0)
#define ZJN_CATCH()  else
#define ZJN_ENDTRY

#endif /* ZUNJIN_H */