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
 * zunjin 语言 - 模块系统实现
 * 拆分自 vm.c：模块缓存、搜索路径、命名空间执行
 * 设计思路参考主流脚本语言的 import 机制，但为完全独立的 C 实现
 * ========================================================================== */

#include "vm.h"
#include "lexer.h"
#include "parser.h"
#include "ast.h"
#include "output.h"

#include <string.h>
#include <stdio.h>

#ifdef _WIN32
#include <windows.h>
#include <direct.h>
#else
#include <unistd.h>
#endif

/* ===================================================================
 *  模块搜索辅助函数
 * =================================================================== */

/* 可执行文件所在目录（模块搜索基准） */
static void module_exe_dir(char* buf, int cap) {
    buf[0] = '\0';
#ifdef _WIN32
    GetModuleFileNameA(NULL, buf, cap);
    char* slash = strrchr(buf, '\\');
    if (slash) *slash = '\0';
#else
    (void)cap;
    /* 非 Windows 平台回退：当前目录 */
    strcpy(buf, ".");
#endif
}

/* 读取整个文件（跳过 UTF-8 BOM），返回 malloc 字符串 */
static char* module_read_file(const char* path, long* out_size) {
    FILE* fp = fopen(path, "rb");
    if (!fp) return NULL;
    fseek(fp, 0, SEEK_END);
    long size = ftell(fp);
    fseek(fp, 0, SEEK_SET);
    if (size < 0 || size > ZUNJIN_MAX_SOURCE) {
        fclose(fp);
        return NULL;
    }
    char* buf = (char*)malloc((size_t)size + 1);
    if (!buf) { fclose(fp); return NULL; }
    long rd = (long)fread(buf, 1, (size_t)size, fp);
    buf[rd] = '\0';
    fclose(fp);
    long off = 0;
    if (rd >= 3 && (unsigned char)buf[0] == 0xEF &&
        (unsigned char)buf[1] == 0xBB && (unsigned char)buf[2] == 0xBF)
        off = 3;
    if (off > 0) {
        memmove(buf, buf + off, (size_t)(rd - off + 1));
        rd -= off;
    }
    if (out_size) *out_size = rd;
    return buf;
}

/* 文件是否存在 */
static int module_file_exists(const char* path) {
    FILE* f = fopen(path, "rb");
    if (f) { fclose(f); return 1; }
    return 0;
}

/* 按搜索顺序解析模块文件路径；找到返回 1 */
static int module_resolve(const char* name, char* out, int cap) {
    char exe_dir[1024];
    module_exe_dir(exe_dir, sizeof(exe_dir));

    /* 搜索顺序：
     *   1. 当前目录 <name>.z / <name>/__init__.z
     *   2. 当前目录 StdLib/<name>.z / StdLib/<name>/__init__.z
     *   3. 可执行文件目录 <name>.z / <name>/__init__.z
     *   4. 可执行文件目录 StdLib/<name>.z / StdLib/<name>/__init__.z */
    char dir0[1024] = ".";
    char dir1[1024];
    snprintf(dir1, sizeof(dir1), "StdLib");
    char dir2[1024];
    snprintf(dir2, sizeof(dir2), "%s", exe_dir);
    char dir3[1024];
    snprintf(dir3, sizeof(dir3), "%s/StdLib", exe_dir);

    const char* dirs[4] = { dir0, dir1, dir2, dir3 };
    char cand[1024];
    for (int i = 0; i < 4; i++) {
        snprintf(cand, sizeof(cand), "%s/%s.z", dirs[i], name);
        if (module_file_exists(cand)) {
            snprintf(out, cap, "%s", cand);
            return 1;
        }
        snprintf(cand, sizeof(cand), "%s/%s/__init__.z", dirs[i], name);
        if (module_file_exists(cand)) {
            snprintf(out, cap, "%s", cand);
            return 1;
        }
    }
    return 0;
}

/* ===================================================================
 *  模块导入
 * =================================================================== */

int zjn_vm_import_module(ZjnVM* vm, ZjnEnv* env, const char* name,
                         int line, int col) {
    /* 1. 模块缓存命中（等价 sys.modules 检查） */
    for (int i = 0; i < vm->module_count; i++) {
        ZjnModule* m = vm->modules[i]->data.module_val;
        if (strcmp(m->name, name) == 0) {
            zjn_env_define_var_take(env, name, zjn_val_copy(vm->modules[i]));
            return 0;
        }
    }

    /* 2. 搜索模块文件 */
    char path[1024];
    if (!module_resolve(name, path, sizeof(path))) {
        char msg[480];
        snprintf(msg, sizeof(msg), "模块未找到: %s（检查 StdLib 目录）", name);
        ZJN_THROW(msg, line, col);
    }

    /* 3. 读取源码 */
    long size = 0;
    char* source = module_read_file(path, &size);
    if (!source) {
        char msg[480];
        snprintf(msg, sizeof(msg), "无法读取模块: %s", name);
        ZJN_THROW(msg, line, col);
    }

    /* 4. 词法/语法分析（错误经 TRY 捕获后抛给外层） */
    jmp_buf saved;
    memcpy(saved, zjn_error_buf, sizeof(jmp_buf));

    ZjnLexer* lexer = NULL;
    ZjnParser* parser = NULL;
    ZjnAstNode* program = NULL;
    int failed = 0;

    ZJN_TRY() {
        lexer = zjn_lexer_create(source);
        if (!lexer) ZJN_THROW("模块词法分析器创建失败", line, col);
        parser = zjn_parser_create(lexer);
        if (!parser) ZJN_THROW("模块语法解析器创建失败", line, col);
        program = zjn_parser_parse(parser);
        if (!program) ZJN_THROW("模块解析失败: 未生成 AST", line, col);
    }
    ZJN_CATCH() {
        failed = 1;
    }
    ZJN_ENDTRY;
    memcpy(zjn_error_buf, saved, sizeof(jmp_buf));

    if (failed) {
        char msg[480];
        snprintf(msg, sizeof(msg), "模块 %s 语法错误: %s (行 %d)",
                 name, zjn_last_error.message, zjn_last_error.line);
        if (parser) zjn_parser_free(parser);
        if (lexer) zjn_lexer_free(lexer);
        free(source);
        ZJN_THROW(msg, line, col);
    }

    /* 5. 模块命名空间环境 + 执行模块体 */
    ZjnEnv* ns = zjn_env_create(name, vm->global_env);
    LoopControl lc = LOOP_NONE;
    ReturnValue rv = { 0, NULL };
    failed = 0;
    ZJN_TRY() {
        exec_block(vm, program->data.program.stmts,
                   program->data.program.stmt_count, ns, &lc, &rv);
    }
    ZJN_CATCH() {
        failed = 1;
    }
    ZJN_ENDTRY;
    memcpy(zjn_error_buf, saved, sizeof(jmp_buf));

    if (failed) {
        char msg[480];
        snprintf(msg, sizeof(msg), "模块 %s 执行错误: %s (行 %d)",
                 name, zjn_last_error.message, zjn_last_error.line);
        zjn_env_free(ns);
        if (program) zjn_ast_free(program);
        if (parser) zjn_parser_free(parser);
        if (lexer) zjn_lexer_free(lexer);
        free(source);
        ZJN_THROW(msg, line, col);
    }

    if (rv.value) zjn_val_free(rv.value);

    /* 6. 构造模块值：托管命名空间与 AST（函数体引用 AST 节点） */
    ZjnModule* m = ZUNJIN_ALLOC(ZjnModule);
    if (!m) {
        zjn_env_free(ns);
        zjn_ast_free(program);
        if (parser) zjn_parser_free(parser);
        if (lexer) zjn_lexer_free(lexer);
        free(source);
        ZJN_THROW("内存不足", line, col);
    }
    m->name = strdup(name);
    m->ns = ns;
    m->ast = program;

    ZjnValue* mval = zjn_val_module(m);
    if (vm->module_count < ZJN_MAX_MODULES)
        vm->modules[vm->module_count++] = zjn_val_copy(mval);

    zjn_env_define_var_take(env, name, mval);

    if (parser) zjn_parser_free(parser);
    if (lexer) zjn_lexer_free(lexer);
    free(source);
    return 0;
}