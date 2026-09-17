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
 * zunjin 语言 - 环境/作用域管理接口
 * 支持嵌套作用域和变量查找链
 * 完全独立重新实现
 *
 * 版本 3.0 变更：
 *  - 变量查找增加哈希索引（线性探测），每个作用域 O(1) 平均查找，
 *    替代纯链表线性扫描；节点链表仍保留用于释放与遍历。
 *  - 新增 zjn_env_define_var_take：参数绑定所有权转移，避免
 *    一次多余的引用计数 copy/free（等价于直接帧参数槽直传）。
 *
 * 版本 3.1 变更：
 *  - 函数链同样哈希化：全局环境约 80 个内置函数，每次函数调用
 *    都线性扫描的代价可观，哈希后 O(1) 平均查找。
 * ========================================================================== */

#ifndef ZUNJIN_ENV_H
#define ZUNJIN_ENV_H

#include "zunjin.h"
#include "object.h"

/* ---------- 环境节点 ---------- */
typedef struct ZjnEnvNode {
    char* name;
    ZjnValue* value;
    struct ZjnEnvNode* next;
} ZjnEnvNode;

/* ---------- 环境 ---------- */
struct ZjnEnv {
    char* name;
    ZjnEnvNode* variables;      /* 变量链（保持插入序，用于释放） */
    ZjnEnvNode* functions;
    struct ZjnEnv* parent;
    ZjnEnvNode** var_slots;     /* 变量哈希索引：槽位存节点指针，-1 语义用 NULL */
    int var_slots_cap;          /* 索引容量（2 的幂） */
    int var_count;              /* 变量总数（扩容判定） */
    ZjnEnvNode** func_slots;    /* 函数哈希索引（同变量，加速内置/函数查找） */
    int func_slots_cap;
    int func_count;
};

/* ---------- 函数 ---------- */
ZjnEnv* zjn_env_create(const char* name, ZjnEnv* parent);
void zjn_env_free(ZjnEnv* env);

/* 变量操作（copy 语义：环境持有 +1 引用，调用方保留自己那份） */
void zjn_env_define_var(ZjnEnv* env, const char* name, ZjnValue* value);
/* 变量操作（接管语义：环境直接持有 value，调用方不得再释放） */
void zjn_env_define_var_take(ZjnEnv* env, const char* name, ZjnValue* value);
ZjnValue* zjn_env_get_var(const ZjnEnv* env, const char* name);
void zjn_env_set_var(ZjnEnv* env, const char* name, ZjnValue* value);
int zjn_env_has_var(const ZjnEnv* env, const char* name);

/* 函数操作 —— 使用 zjn_env_set_func（不拷贝，环境接管所有权） */
void zjn_env_set_func(ZjnEnv* env, const char* name, ZjnValue* value);
ZjnValue* zjn_env_get_func(const ZjnEnv* env, const char* name);
int zjn_env_has_func(const ZjnEnv* env, const char* name);

/* 子作用域 */
ZjnEnv* zjn_env_push(ZjnEnv* env, const char* name);
ZjnEnv* zjn_env_pop(ZjnEnv* env);

/* 全局环境初始化 */
ZjnEnv* zjn_env_create_global(void);

#endif /* ZUNJIN_ENV_H */
