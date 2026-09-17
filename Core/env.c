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
 * zunjin 语言 - 环境/作用域管理实现
 * 支持嵌套作用域和变量查找链
 * 完全独立重新实现
 *
 * 版本 3.0：变量查找哈希索引 + 参数所有权转移绑定。
 * ========================================================================== */

#include "env.h"
#include <string.h>
#include <stdio.h>

/* ---------- 字符串哈希（FNV-1a，与字典一致） ---------- */
static unsigned long env_hash(const char* key) {
    unsigned long h = 2166136261UL;
    while (*key) {
        h ^= (unsigned char)*key++;
        h *= 16777619UL;
    }
    return h;
}

/* ---------- 前向声明 ---------- */
static ZjnEnvNode* find_node_linear(ZjnEnvNode* head, const char* name);

/* ---------- 创建环境 ---------- */
ZjnEnv* zjn_env_create(const char* name, ZjnEnv* parent) {
    ZjnEnv* env = ZUNJIN_ALLOC(ZjnEnv);
    if (!env) return NULL;
    env->name = strdup(name);
    env->variables = NULL;
    env->functions = NULL;
    env->parent = parent;
    env->var_slots = NULL;
    env->var_slots_cap = 0;
    env->var_count = 0;
    env->func_slots = NULL;
    env->func_slots_cap = 0;
    env->func_count = 0;
    return env;
}

/* ---------- 释放环境 ---------- */
void zjn_env_free(ZjnEnv* env) {
    if (!env) return;
    if (env->name) free(env->name);

    /* 释放变量链 */
    ZjnEnvNode* node = env->variables;
    while (node) {
        ZjnEnvNode* next = node->next;
        if (node->name) free(node->name);
        if (node->value) zjn_val_free(node->value);
        free(node);
        node = next;
    }

    /* 释放函数链 */
    node = env->functions;
    while (node) {
        ZjnEnvNode* next = node->next;
        if (node->name) free(node->name);
        if (node->value) zjn_val_free(node->value);
        free(node);
        node = next;
    }

    free(env->var_slots);
    free(env->func_slots);
    free(env);
}

/* ===================================================================
 *  变量哈希索引
 * =================================================================== */

/* 扩容并重哈希全部变量节点 */
static void env_grow_slots(ZjnEnv* env) {
    int new_cap = env->var_slots_cap == 0 ? 8 : env->var_slots_cap * 2;
    ZjnEnvNode** ns = (ZjnEnvNode**)calloc(new_cap, sizeof(ZjnEnvNode*));
    if (!ns) return;
    int mask = new_cap - 1;
    ZjnEnvNode* node = env->variables;
    while (node) {
        int slot = (int)(env_hash(node->name) & (unsigned long)mask);
        while (ns[slot]) slot = (slot + 1) & mask;
        ns[slot] = node;
        node = node->next;
    }
    free(env->var_slots);
    env->var_slots = ns;
    env->var_slots_cap = new_cap;
}

/* 在当前作用域内查找变量节点（仅本层，不沿父链） */
static ZjnEnvNode* find_node_local(ZjnEnv* env, const char* name) {
    if (!env->var_slots || env->var_count == 0)
        return find_node_linear(env->variables, name);
    int mask = env->var_slots_cap - 1;
    int slot = (int)(env_hash(name) & (unsigned long)mask);
    for (int probe = 0; probe < env->var_slots_cap; probe++) {
        ZjnEnvNode* node = env->var_slots[slot];
        if (!node) break;
        if (strcmp(node->name, name) == 0) return node;
        slot = (slot + 1) & mask;
    }
    /* 哈希未命中时线性兜底（保证极端/内存不足场景正确） */
    return find_node_linear(env->variables, name);
}

/* 线性查找（兼容旧代码路径，仅在无索引时使用） */
static ZjnEnvNode* find_node_linear(ZjnEnvNode* head, const char* name) {
    while (head) {
        if (strcmp(head->name, name) == 0) return head;
        head = head->next;
    }
    return NULL;
}

static ZjnEnvNode* add_node(ZjnEnvNode** head, const char* name,
                             ZjnValue* value) {
    ZjnEnvNode* node = ZUNJIN_ALLOC(ZjnEnvNode);
    if (!node) return NULL;
    node->name = strdup(name);
    node->value = value;
    node->next = *head;
    *head = node;
    return node;
}

/* ---------- 变量操作 ---------- */
void zjn_env_define_var(ZjnEnv* env, const char* name, ZjnValue* value) {
    if (!env) return;
    ZjnEnvNode* existing = find_node_local(env, name);
    if (existing) {
        if (existing->value) zjn_val_free(existing->value);
        existing->value = value ? zjn_val_copy(value) : zjn_val_none();
        return;
    }
    ZjnEnvNode* node = add_node(&env->variables, name,
                                value ? zjn_val_copy(value) : zjn_val_none());
    if (!node) return;
    env->var_count++;
    if (env->var_slots_cap == 0 || env->var_count * 3 >= env->var_slots_cap * 2)
        env_grow_slots(env);
    /* 新节点插入哈希槽（add_node 头插后变量链已含它） */
    if (env->var_slots) {
        int mask = env->var_slots_cap - 1;
        int slot = (int)(env_hash(node->name) & (unsigned long)mask);
        while (env->var_slots[slot]) slot = (slot + 1) & mask;
        env->var_slots[slot] = node;
    }
}

/* 接管所有权：环境直接持有 value（refs 不再 +1），调用方不得再释放 */
void zjn_env_define_var_take(ZjnEnv* env, const char* name, ZjnValue* value) {
    if (!env) { if (value) zjn_val_free(value); return; }
    ZjnEnvNode* existing = find_node_local(env, name);
    if (existing) {
        if (existing->value) zjn_val_free(existing->value);
        existing->value = value ? value : zjn_val_none();
        return;
    }
    ZjnEnvNode* node = add_node(&env->variables, name,
                                value ? value : zjn_val_none());
    if (!node) return;
    env->var_count++;
    if (env->var_slots_cap == 0 || env->var_count * 3 >= env->var_slots_cap * 2)
        env_grow_slots(env);
    if (env->var_slots) {
        int mask = env->var_slots_cap - 1;
        int slot = (int)(env_hash(node->name) & (unsigned long)mask);
        while (env->var_slots[slot]) slot = (slot + 1) & mask;
        env->var_slots[slot] = node;
    }
}

ZjnValue* zjn_env_get_var(const ZjnEnv* env, const char* name) {
    const ZjnEnv* current = env;
    while (current) {
        ZjnEnvNode* node = find_node_local((ZjnEnv*)current, name);
        if (node) return node->value;
        current = current->parent;
    }
    return NULL;
}

void zjn_env_set_var(ZjnEnv* env, const char* name, ZjnValue* value) {
    ZjnEnv* current = env;
    while (current) {
        ZjnEnvNode* node = find_node_local(current, name);
        if (node) {
            if (node->value) zjn_val_free(node->value);
            node->value = value ? zjn_val_copy(value) : zjn_val_none();
            return;
        }
        current = current->parent;
    }
    zjn_env_define_var(env, name, value);
}

int zjn_env_has_var(const ZjnEnv* env, const char* name) {
    return zjn_env_get_var(env, name) != NULL;
}

/* ===================================================================
 *  函数哈希索引（与变量索引同构）
 * =================================================================== */

static void env_grow_func_slots(ZjnEnv* env) {
    int new_cap = env->func_slots_cap == 0 ? 16 : env->func_slots_cap * 2;
    ZjnEnvNode** ns = (ZjnEnvNode**)calloc(new_cap, sizeof(ZjnEnvNode*));
    if (!ns) return;
    int mask = new_cap - 1;
    ZjnEnvNode* node = env->functions;
    while (node) {
        int slot = (int)(env_hash(node->name) & (unsigned long)mask);
        while (ns[slot]) slot = (slot + 1) & mask;
        ns[slot] = node;
        node = node->next;
    }
    free(env->func_slots);
    env->func_slots = ns;
    env->func_slots_cap = new_cap;
}

/* 在当前作用域内查找函数节点（仅本层，不沿父链） */
static ZjnEnvNode* find_func_local(ZjnEnv* env, const char* name) {
    if (!env->func_slots || env->func_count == 0)
        return find_node_linear(env->functions, name);
    int mask = env->func_slots_cap - 1;
    int slot = (int)(env_hash(name) & (unsigned long)mask);
    for (int probe = 0; probe < env->func_slots_cap; probe++) {
        ZjnEnvNode* node = env->func_slots[slot];
        if (!node) break;
        if (strcmp(node->name, name) == 0) return node;
        slot = (slot + 1) & mask;
    }
    /* 哈希未命中时线性兜底 */
    return find_node_linear(env->functions, name);
}

/* 函数操作 —— 使用 zjn_env_set_func 代替（不拷贝，环境接管所有权） */
void zjn_env_set_func(ZjnEnv* env, const char* name, ZjnValue* value) {
    ZjnEnvNode* existing = find_func_local(env, name);
    if (existing) {
        if (existing->value) zjn_val_free(existing->value);
        existing->value = value;
        return;
    }
    ZjnEnvNode* node = add_node(&env->functions, name, value);
    if (!node) return;
    env->func_count++;
    if (env->func_slots_cap == 0 || env->func_count * 3 >= env->func_slots_cap * 2)
        env_grow_func_slots(env);
    if (env->func_slots) {
        int mask = env->func_slots_cap - 1;
        int slot = (int)(env_hash(node->name) & (unsigned long)mask);
        while (env->func_slots[slot]) slot = (slot + 1) & mask;
        env->func_slots[slot] = node;
    }
}

ZjnValue* zjn_env_get_func(const ZjnEnv* env, const char* name) {
    const ZjnEnv* current = env;
    while (current) {
        ZjnEnvNode* node = find_func_local((ZjnEnv*)current, name);
        if (node) return node->value;
        current = current->parent;
    }
    return NULL;
}

int zjn_env_has_func(const ZjnEnv* env, const char* name) {
    return zjn_env_get_func(env, name) != NULL;
}

/* ---------- 作用域推入推出 ---------- */
ZjnEnv* zjn_env_push(ZjnEnv* env, const char* name) {
    return zjn_env_create(name, env);
}

ZjnEnv* zjn_env_pop(ZjnEnv* env) {
    ZjnEnv* parent = env->parent;
    return parent;
}

/* ---------- 创建全局环境 ---------- */
ZjnEnv* zjn_env_create_global(void) {
    ZjnEnv* env = zjn_env_create("global", NULL);
    return env;
}
