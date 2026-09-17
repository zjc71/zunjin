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
 * zunjin 语言 - 主入口程序
 * 命令行：zunjin <脚本文件.z>   运行脚本
 *         zunjin               进入交互式 REPL
 * 完全独立重新实现
 * ========================================================================== */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#ifdef _WIN32
#include <windows.h>
#endif

#include "zunjin.h"
#include "lexer.h"
#include "parser.h"
#include "vm.h"
#include "output.h"

/* ---------- 读取文件 ---------- */
static char* read_file(const char* path, long* out_size) {
    FILE* fp = fopen(path, "rb");
    if (!fp) {
        fprintf(stderr, "错误: 无法打开文件: %s\n", path);
        return NULL;
    }

    fseek(fp, 0, SEEK_END);
    long size = ftell(fp);
    fseek(fp, 0, SEEK_SET);

    if (size > ZUNJIN_MAX_SOURCE) {
        fprintf(stderr, "错误: 文件过大 (最大 %d 字节)\n", ZUNJIN_MAX_SOURCE);
        fclose(fp);
        return NULL;
    }

    char* buffer = (char*)malloc((size_t)size + 1);
    if (!buffer) {
        fprintf(stderr, "错误: 内存不足\n");
        fclose(fp);
        return NULL;
    }

    long read_size = (long)fread(buffer, 1, (size_t)size, fp);
    buffer[read_size] = '\0';
    fclose(fp);

    /* 跳过 UTF-8 BOM (0xEF 0xBB 0xBF) */
    long offset = 0;
    if (read_size >= 3 &&
        (unsigned char)buffer[0] == 0xEF &&
        (unsigned char)buffer[1] == 0xBB &&
        (unsigned char)buffer[2] == 0xBF) {
        offset = 3;
    }

    if (offset > 0) {
        memmove(buffer, buffer + offset, (size_t)(read_size - offset + 1));
        read_size -= offset;
    }

    if (out_size) *out_size = read_size;
    return buffer;
}

/* ---------- 解析并执行一段源码（共享 VM 环境） ---------- */
static void execute_source(const char* source, ZjnVM* vm, ZjnOutput* output) {
    ZjnLexer* lexer = zjn_lexer_create(source);
    if (!lexer) {
        zjn_output_error(output, "词法分析器创建失败");
        return;
    }

    ZjnParser* parser = zjn_parser_create(lexer);
    if (!parser) {
        zjn_output_error(output, "语法解析器创建失败");
        zjn_lexer_free(lexer);
        return;
    }

    ZjnAstNode* program = NULL;

    ZJN_TRY() {
        program = zjn_parser_parse(parser);
    }
    ZJN_CATCH() {
        zjn_output_error(output, "语法错误: %s (行 %d, 列 %d)",
                         zjn_last_error.message,
                         zjn_last_error.line,
                         zjn_last_error.column);
        zjn_parser_free(parser);
        zjn_lexer_free(lexer);
        return;
    }
    ZJN_ENDTRY;

    if (!program) {
        zjn_output_error(output, "解析失败: 未生成 AST");
        zjn_parser_free(parser);
        zjn_lexer_free(lexer);
        return;
    }

    zjn_vm_execute(vm, program);

    zjn_ast_free(program);
    zjn_parser_free(parser);
    zjn_lexer_free(lexer);
}

/* ---------- 运行脚本文件 ---------- */
static int run_script(const char* source, const char* filename) {
    (void)filename;
    ZjnOutput* output = zjn_output_create(stdout);
    ZjnVM* vm = zjn_vm_create(output);
    if (!vm) {
        zjn_output_error(output, "虚拟机创建失败");
        zjn_output_free(output);
        return 1;
    }
    execute_source(source, vm, output);
    zjn_vm_free(vm);
    zjn_output_free(output);
    return 0;
}

/* ---------- 交互式 REPL ---------- */
static void repl_loop(void) {
    ZjnOutput* output = zjn_output_create(stdout);
    ZjnVM* vm = zjn_vm_create(output);
    if (!vm) {
        zjn_output_error(output, "虚拟机创建失败");
        zjn_output_free(output);
        return;
    }

    printf("zunjin 交互模式 v%s（输入 exit 或 quit 退出）\n", ZUNJIN_VERSION);
    printf("支持多行输入：以冒号结尾的语句进入代码块，块内行以空格缩进\n");
    printf("----------------------------------------\n");

    char line[ZUNJIN_MAX_LINE];
    char* buffer = (char*)malloc(ZUNJIN_MAX_SOURCE);
    if (!buffer) {
        zjn_vm_free(vm);
        zjn_output_free(output);
        return;
    }
    buffer[0] = '\0';
    size_t buf_len = 0;

    while (1) {
        printf(buf_len == 0 ? "zunjin> " : "   ... ");
        fflush(stdout);

        if (!fgets(line, sizeof(line), stdin)) {
            printf("\n");
            break;
        }

        /* 去除末尾换行符 */
        size_t llen = strlen(line);
        while (llen > 0 && (line[llen - 1] == '\n' || line[llen - 1] == '\r'))
            line[--llen] = '\0';

        /* 退出命令 */
        if (buf_len == 0 &&
            (strcmp(line, "exit") == 0 || strcmp(line, "quit") == 0)) {
            break;
        }

        /* 空行且已有累积内容：提交执行 */
        if (llen == 0 && buf_len > 0) {
            execute_source(buffer, vm, output);
            buf_len = 0;
            buffer[0] = '\0';
            continue;
        }
        if (llen == 0) continue;

        /* 累积当前行 */
        if (buf_len + llen + 2 < ZUNJIN_MAX_SOURCE) {
            if (buf_len > 0) buffer[buf_len++] = '\n';
            memcpy(buffer + buf_len, line, llen);
            buf_len += llen;
            buffer[buf_len] = '\0';
        } else {
            zjn_output_error(output, "输入过长");
            buf_len = 0;
            buffer[0] = '\0';
            continue;
        }

        /* 判断是否需要继续输入：
         *  1) 行尾是冒号（进入代码块）
         *  2) 行以空白缩进开头（块内语句）
         *  3) 圆括号/方括号未闭合 */
        int needs_more = 0;
        if (llen > 0 && line[llen - 1] == ':') needs_more = 1;
        int indent = 0;
        while (indent < (int)llen &&
               (line[indent] == ' ' || line[indent] == '\t')) indent++;
        if (indent > 0) needs_more = 1;

        int balance = 0;
        for (size_t i = 0; i < llen; i++) {
            if (line[i] == '(' || line[i] == '[' || line[i] == '{') balance++;
            else if (line[i] == ')' || line[i] == ']' || line[i] == '}') balance--;
        }
        if (balance > 0) needs_more = 1;

        if (needs_more) continue;

        /* 提交执行 */
        execute_source(buffer, vm, output);
        buf_len = 0;
        buffer[0] = '\0';
    }

    free(buffer);
    zjn_vm_free(vm);
    zjn_output_free(output);
}

/* ---------- 主函数 ---------- */
int main(int argc, char* argv[]) {
#ifdef _WIN32
    /* 设置控制台输出为 UTF-8，解决中文乱码 */
    SetConsoleOutputCP(CP_UTF8);
    SetConsoleCP(CP_UTF8);
#endif

    if (argc < 2) {
        repl_loop();
        return 0;
    }

    const char* path = argv[1];

    /* 检查扩展名 */
    const char* ext = strrchr(path, '.');
    if (!ext || strcmp(ext, ".z") != 0) {
        fprintf(stderr, "警告: 文件扩展名不是 .z\n");
    }

    /* 读取文件 */
    long size = 0;
    char* source = read_file(path, &size);
    if (!source) {
        return 1;
    }

    fprintf(stdout, "zunjin v%s | 运行: %s\n", ZUNJIN_VERSION, path);
    fprintf(stdout, "----------------------------------------\n");

    /* 执行 */
    int result = run_script(source, path);

    free(source);
    return result;
}
