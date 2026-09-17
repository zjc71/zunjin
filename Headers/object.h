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
 * zunjin 语言 - 运行时对象系统
 * 定义所有运行时值的类型体系
 * 完全独立重新实现
 *
 * 版本 2.0 变更：
 *  - 引入引用计数：copy 为 O(1) 共享（+1），free 为 -1，归零才释放，
 *    修复列表/函数值被提前释放导致的悬垂指针与双重释放。
 *  - 新增字典（dict）类型：{} 字面量、键访问、in 判断。
 *  - 新增切片与统一索引辅助（支持负索引）。
 * ========================================================================== */

#ifndef ZUNJIN_OBJECT_H
#define ZUNJIN_OBJECT_H

#include "zunjin.h"

/* ---------- 值类型枚举 ---------- */
typedef enum {
    ZVAL_NONE,      /* 空值 */
    ZVAL_BOOL,      /* 布尔 */
    ZVAL_INT,       /* 整数 */
    ZVAL_FLOAT,     /* 浮点数 */
    ZVAL_STRING,    /* 字符串 */
    ZVAL_LIST,      /* 列表 */
    ZVAL_DICT,      /* 字典 */
    ZVAL_FUNC,      /* 用户函数 */
    ZVAL_BUILTIN,   /* 内置函数 */
    ZVAL_MODULE,    /* 模块（use 导入的命名空间） */
} ZjnValueType;

/* ---------- 函数对象（前向声明） ---------- */
typedef struct ZjnFunc {
    char* name;
    char** params;
    int param_count;
    struct ZjnAstNode** defaults;   /* 默认参数表达式，可为 NULL；与 params 对齐 */
    struct ZjnAstNode* body;        /* 函数体 AST 块 */
    struct ZjnEnv* closure;         /* 定义时所在环境（等价于全局环境）：
                                     * 模块函数指向模块命名空间，顶层函数指向全局环境 */
} ZjnFunc;

/* ---------- 内置函数对象 ---------- */
typedef struct ZjnBuiltin {
    char* name;
    int arity;         /* -1 表示可变参数 */
    ZjnValue* (*func)(int argc, ZjnValue** args);
} ZjnBuiltin;

/* ---------- 模块对象（use 导入） ----------
 * ns  为模块命名空间环境；ast 为模块源码 AST。
 * 模块内函数体引用 ast 节点，因此 ast 由模块值托管生命周期。 */
typedef struct ZjnModule {
    char* name;
    struct ZjnEnv* ns;
    struct ZjnAstNode* ast;
} ZjnModule;

/* ---------- 列表对象 ---------- */
typedef struct ZjnList {
    ZjnValue** items;
    int count;
    int capacity;
} ZjnList;

/* ---------- 字典键值对 ---------- */
typedef struct ZjnDictEntry {
    char* key;
    ZjnValue* value;
} ZjnDictEntry;

/* ---------- 字典对象 ----------
 * 有序哈希表设计（独立实现，紧凑字典思路）：
 *  - entries 数组保持插入顺序（遍历/打印/foreach 顺序稳定）
 *  - index 哈希索引（2 的幂容量 + 线性探测）提供 O(1) 平均查找
 *  - 删除时前移 entries 并整体重哈希（删除频率低，摊还代价可接受） */
typedef struct ZjnDict {
    ZjnDictEntry* entries;
    int count;
    int capacity;
    int* index;          /* 哈希槽 → entries 下标；-1 表示空槽 */
    int index_cap;       /* 索引容量（2 的幂） */
} ZjnDict;

/* ---------- 运行时值 ---------- */
struct ZjnValue {
    ZjnValueType type;
    int refs;            /* 引用计数：copy +1，free -1，归零释放全部资源 */
    union {
        int      bool_val;
        long     int_val;
        double   float_val;
        char*    string_val;
        ZjnList* list_val;
        ZjnDict* dict_val;
        ZjnFunc* func_val;
        ZjnBuiltin* builtin_val;
        ZjnModule* module_val;
    } data;
};

/* ---------- 值创建函数（创建后 refs = 1） ---------- */
ZjnValue* zjn_val_none(void);
ZjnValue* zjn_val_bool(int val);
ZjnValue* zjn_val_int(long val);
ZjnValue* zjn_val_float(double val);
ZjnValue* zjn_val_string(const char* val);
ZjnValue* zjn_val_string_from(const char* val, int len);
ZjnValue* zjn_val_list(void);
ZjnValue* zjn_val_dict(void);
ZjnValue* zjn_val_func(ZjnFunc* func);
ZjnValue* zjn_val_builtin(ZjnBuiltin* builtin);
ZjnValue* zjn_val_module(ZjnModule* module);

/* ---------- 值操作 ---------- */
ZjnValue* zjn_val_copy(const ZjnValue* src);   /* 引用共享 +1 */
ZjnValue* zjn_val_clone(const ZjnValue* src);  /* 深拷贝（dict/list 递归） */
void zjn_val_free(ZjnValue* val);              /* 引用 -1，归零释放 */
void zjn_val_print(const ZjnValue* val, FILE* stream);

/* ---------- 列表操作 ---------- */
void zjn_list_append(ZjnValue* list, ZjnValue* item);
ZjnValue* zjn_list_get(const ZjnValue* list, int index);
int zjn_list_len(const ZjnValue* list);

/* ---------- 字典操作 ---------- */
void zjn_dict_set(ZjnValue* dict, const char* key, ZjnValue* value); /* 接管 value 引用 */
ZjnValue* zjn_dict_get(const ZjnValue* dict, const char* key);       /* 借出，勿释放 */
int zjn_dict_has(const ZjnValue* dict, const char* key);
void zjn_dict_remove(ZjnValue* dict, const char* key);
void zjn_dict_clear(ZjnValue* dict);
int zjn_dict_len(const ZjnValue* dict);

/* ---------- 字符串操作 ---------- */
int zjn_string_len(const ZjnValue* val);
ZjnValue* zjn_string_concat(const ZjnValue* a, const ZjnValue* b);
ZjnValue* zjn_string_repeat(const ZjnValue* str, long times);
ZjnValue* zjn_string_sub(const ZjnValue* str, int start, int len);   /* 子串（0 基） */

/* ---------- 序列通用操作（列表/字符串/字典） ---------- */
ZjnValue* zjn_val_len(const ZjnValue* val);                          /* size() 后端 */
int zjn_val_contains(const ZjnValue* container, const ZjnValue* item); /* in 运算 */
ZjnValue* zjn_val_index(const ZjnValue* container, int index);       /* 统一索引（负索引归一） */
ZjnValue* zjn_val_slice(const ZjnValue* container, int start, int stop, int step);
ZjnValue* zjn_list_concat(const ZjnValue* a, const ZjnValue* b);
ZjnValue* zjn_list_repeat(const ZjnValue* list, long times);

/* 索引归一化：负索引转为非负；越界返回 -1 表示无效 */
int zjn_normalize_index(int index, int len);

/* ---------- 类型检查 ---------- */
#define ZJN_IS_NONE(v)     ((v)->type == ZVAL_NONE)
#define ZJN_IS_BOOL(v)     ((v)->type == ZVAL_BOOL)
#define ZJN_IS_INT(v)      ((v)->type == ZVAL_INT)
#define ZJN_IS_FLOAT(v)    ((v)->type == ZVAL_FLOAT)
#define ZJN_IS_NUMBER(v)   ((v)->type == ZVAL_INT || (v)->type == ZVAL_FLOAT)
#define ZJN_IS_STRING(v)   ((v)->type == ZVAL_STRING)
#define ZJN_IS_LIST(v)     ((v)->type == ZVAL_LIST)
#define ZJN_IS_DICT(v)     ((v)->type == ZVAL_DICT)
#define ZJN_IS_FUNC(v)     ((v)->type == ZVAL_FUNC)
#define ZJN_IS_BUILTIN(v)  ((v)->type == ZVAL_BUILTIN)
#define ZJN_IS_MODULE(v)   ((v)->type == ZVAL_MODULE)

/* ---------- 值转换 ---------- */
int      zjn_val_to_bool(const ZjnValue* val);
long     zjn_val_to_int(const ZjnValue* val);
double   zjn_val_to_float(const ZjnValue* val);
const char* zjn_val_to_string(const ZjnValue* val);

/* 判断真值（用于条件判断） */
int zjn_val_is_truthy(const ZjnValue* val);

/* 值比较：返回 -1/0/1（数值按数值、字符串按字典序） */
int zjn_val_compare(const ZjnValue* a, const ZjnValue* b);
int zjn_val_equal(const ZjnValue* a, const ZjnValue* b);

#endif /* ZUNJIN_OBJECT_H */
