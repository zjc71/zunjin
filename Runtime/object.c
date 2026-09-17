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
 * zunjin 语言 - 运行时对象系统实现
 * 完全独立重新实现
 *
 * 版本 2.0：引用计数语义
 *  - zjn_val_copy：引用 +1（O(1) 共享，与 zunjin 引用语义一致）
 *  - zjn_val_free：引用 -1，归零才释放资源
 *  - 列表/字符串/字典均为共享可变对象；函数对象由环境统一托管
 * ========================================================================== */

#include "object.h"
#include "env.h"
#include "ast.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdarg.h>

/* ---------- 全局错误缓冲区 ---------- */
jmp_buf zjn_error_buf;
ZjnError zjn_last_error;

/* ===================================================================
 *  值创建函数
 * =================================================================== */

/* ---------- 值结构空闲链表 ----------
 * 引用计数归零释放后，值结构本身进空闲链表复用，
 * 大幅减少算术热循环中的 malloc/free 次数（等价于
 * 对小对象的 free-list 思路，独立实现）。 */
#define ZJN_VAL_FREE_MAX 4096
static ZjnValue* g_val_free_list = NULL;
static int g_val_free_count = 0;

static ZjnValue* alloc_val(ZjnValueType type) {
    ZjnValue* v = NULL;
    if (g_val_free_list) {
        v = g_val_free_list;
        g_val_free_list = (ZjnValue*)(void*)v->data.func_val; /* 借指针字段作 next */
        g_val_free_count--;
    } else {
        v = (ZjnValue*)malloc(sizeof(ZjnValue));
        if (!v) return NULL;
    }
    v->type = type;
    v->refs = 1;
    memset(&v->data, 0, sizeof(v->data));
    return v;
}

/* 归还结构体到空闲链表（仅当负载已释放完毕） */
static void val_free_struct(ZjnValue* val) {
    if (g_val_free_count < ZJN_VAL_FREE_MAX) {
        val->data.func_val = (ZjnFunc*)(void*)g_val_free_list;
        g_val_free_list = val;
        g_val_free_count++;
    } else {
        free(val);
    }
}

ZjnValue* zjn_val_none(void) {
    return alloc_val(ZVAL_NONE);
}

ZjnValue* zjn_val_bool(int val) {
    ZjnValue* v = alloc_val(ZVAL_BOOL);
    if (!v) return NULL;
    v->data.bool_val = val ? 1 : 0;
    return v;
}

ZjnValue* zjn_val_int(long val) {
    ZjnValue* v = alloc_val(ZVAL_INT);
    if (!v) return NULL;
    v->data.int_val = val;
    return v;
}

ZjnValue* zjn_val_float(double val) {
    ZjnValue* v = alloc_val(ZVAL_FLOAT);
    if (!v) return NULL;
    v->data.float_val = val;
    return v;
}

ZjnValue* zjn_val_string(const char* val) {
    return zjn_val_string_from(val, (int)strlen(val));
}

ZjnValue* zjn_val_string_from(const char* val, int len) {
    ZjnValue* v = alloc_val(ZVAL_STRING);
    if (!v) return NULL;
    v->data.string_val = (char*)malloc(len + 1);
    if (v->data.string_val) {
        memcpy(v->data.string_val, val, len);
        v->data.string_val[len] = '\0';
    }
    return v;
}

ZjnValue* zjn_val_list(void) {
    ZjnValue* v = alloc_val(ZVAL_LIST);
    if (!v) return NULL;
    ZjnList* lst = ZUNJIN_ALLOC(ZjnList);
    if (!lst) { free(v); return NULL; }
    lst->items = NULL;
    lst->count = 0;
    lst->capacity = 0;
    v->data.list_val = lst;
    return v;
}

ZjnValue* zjn_val_dict(void) {
    ZjnValue* v = alloc_val(ZVAL_DICT);
    if (!v) return NULL;
    ZjnDict* d = ZUNJIN_ALLOC(ZjnDict);
    if (!d) { free(v); return NULL; }
    d->entries = NULL;
    d->count = 0;
    d->capacity = 0;
    d->index = NULL;
    d->index_cap = 0;
    v->data.dict_val = d;
    return v;
}

ZjnValue* zjn_val_func(ZjnFunc* func) {
    ZjnValue* v = alloc_val(ZVAL_FUNC);
    if (!v) return NULL;
    v->data.func_val = func;
    return v;
}

ZjnValue* zjn_val_builtin(ZjnBuiltin* builtin) {
    ZjnValue* v = alloc_val(ZVAL_BUILTIN);
    if (!v) return NULL;
    v->data.builtin_val = builtin;
    return v;
}

ZjnValue* zjn_val_module(ZjnModule* module) {
    ZjnValue* v = alloc_val(ZVAL_MODULE);
    if (!v) return NULL;
    v->data.module_val = module;
    return v;
}

/* ===================================================================
 *  值复制与释放（引用计数）
 * =================================================================== */

ZjnValue* zjn_val_copy(const ZjnValue* src) {
    if (!src) return NULL;
    /* 引用共享：仅增加计数，不复制数据 */
    ((ZjnValue*)src)->refs++;
    return (ZjnValue*)src;
}

/* 深拷贝：仅用于需要独立副本的场景（如 REPL 历史） */
ZjnValue* zjn_val_clone(const ZjnValue* src) {
    if (!src) return NULL;
    switch (src->type) {
        case ZVAL_NONE:   return zjn_val_none();
        case ZVAL_BOOL:   return zjn_val_bool(src->data.bool_val);
        case ZVAL_INT:    return zjn_val_int(src->data.int_val);
        case ZVAL_FLOAT:  return zjn_val_float(src->data.float_val);
        case ZVAL_STRING: return zjn_val_string(src->data.string_val);
        case ZVAL_LIST: {
            ZjnValue* v = zjn_val_list();
            ZjnList* src_lst = src->data.list_val;
            for (int i = 0; i < src_lst->count; i++)
                zjn_list_append(v, zjn_val_clone(src_lst->items[i]));
            return v;
        }
        case ZVAL_DICT: {
            ZjnValue* v = zjn_val_dict();
            ZjnDict* src_d = src->data.dict_val;
            for (int i = 0; i < src_d->count; i++)
                zjn_dict_set(v, src_d->entries[i].key,
                             zjn_val_clone(src_d->entries[i].value));
            return v;
        }
        default:
            /* 函数/内置函数：引用共享 */
            return zjn_val_copy(src);
    }
}

void zjn_val_free(ZjnValue* val) {
    if (!val) return;
    if (val->refs > 1) {
        val->refs--;
        return;
    }
    switch (val->type) {
        case ZVAL_STRING:
            free(val->data.string_val);
            break;
        case ZVAL_LIST: {
            ZjnList* lst = val->data.list_val;
            if (lst) {
                for (int i = 0; i < lst->count; i++)
                    zjn_val_free(lst->items[i]);
                free(lst->items);
                free(lst);
            }
            break;
        }
        case ZVAL_DICT: {
            ZjnDict* d = val->data.dict_val;
            if (d) {
                for (int i = 0; i < d->count; i++) {
                    free(d->entries[i].key);
                    zjn_val_free(d->entries[i].value);
                }
                free(d->entries);
                free(d->index);
                free(d);
            }
            break;
        }
        case ZVAL_FUNC: {
            ZjnFunc* f = val->data.func_val;
            if (f) {
                free(f->name);
                /* params/body/defaults 由 AST 树管理，此处不释放；
                 * 函数对象的生命周期由环境统一托管 */
                free(f);
            }
            break;
        }
        case ZVAL_BUILTIN: {
            ZjnBuiltin* b = val->data.builtin_val;
            if (b) {
                free(b->name);
                free(b);
            }
            break;
        }
        case ZVAL_MODULE: {
            ZjnModule* m = val->data.module_val;
            if (m) {
                if (m->name) free(m->name);
                /* ns 环境释放函数值（ZjnFunc 结构）；ast 释放函数体节点 */
                if (m->ns) zjn_env_free(m->ns);
                if (m->ast) zjn_ast_free(m->ast);
                free(m);
            }
            break;
        }
        default:
            break;
    }
    val_free_struct(val);
}

/* ===================================================================
 *  值打印
 * =================================================================== */

void zjn_val_print(const ZjnValue* val, FILE* stream) {
    if (!val) { fprintf(stream, "null"); return; }
    switch (val->type) {
        case ZVAL_NONE:
            fprintf(stream, "null");
            break;
        case ZVAL_BOOL:
            fprintf(stream, "%s", val->data.bool_val ? "yes" : "no");
            break;
        case ZVAL_INT:
            fprintf(stream, "%ld", val->data.int_val);
            break;
        case ZVAL_FLOAT: {
            double d = val->data.float_val;
            if (d == (long)d && d < 1e15 && d > -1e15) {
                fprintf(stream, "%.1f", d);
            } else {
                char buf[64];
                snprintf(buf, sizeof(buf), "%.10g", d);
                fprintf(stream, "%s", buf);
            }
            break;
        }
        case ZVAL_STRING:
            fprintf(stream, "%s", val->data.string_val);
            break;
        case ZVAL_LIST: {
            ZjnList* lst = val->data.list_val;
            fprintf(stream, "[");
            for (int i = 0; i < lst->count; i++) {
                if (i > 0) fprintf(stream, ", ");
                zjn_val_print(lst->items[i], stream);
            }
            fprintf(stream, "]");
            break;
        }
        case ZVAL_DICT: {
            ZjnDict* d = val->data.dict_val;
            fprintf(stream, "{");
            for (int i = 0; i < d->count; i++) {
                if (i > 0) fprintf(stream, ", ");
                fprintf(stream, "%s: ", d->entries[i].key);
                zjn_val_print(d->entries[i].value, stream);
            }
            fprintf(stream, "}");
            break;
        }
        case ZVAL_FUNC:
            fprintf(stream, "<函数 %s>", val->data.func_val->name);
            break;
        case ZVAL_BUILTIN:
            fprintf(stream, "<内置函数 %s>", val->data.builtin_val->name);
            break;
        case ZVAL_MODULE:
            fprintf(stream, "<模块 %s>", val->data.module_val->name);
            break;
    }
}

/* ===================================================================
 *  列表操作
 * =================================================================== */

void zjn_list_append(ZjnValue* list, ZjnValue* item) {
    if (!list || list->type != ZVAL_LIST) return;
    ZjnList* lst = list->data.list_val;
    if (lst->count >= lst->capacity) {
        int new_cap = lst->capacity == 0 ? 8 : lst->capacity * 2;
        ZjnValue** new_items = (ZjnValue**)realloc(
            lst->items, new_cap * sizeof(ZjnValue*));
        if (!new_items) return;
        lst->items = new_items;
        lst->capacity = new_cap;
    }
    lst->items[lst->count++] = item;
}

ZjnValue* zjn_list_get(const ZjnValue* list, int index) {
    if (!list || list->type != ZVAL_LIST) return NULL;
    ZjnList* lst = list->data.list_val;
    index = zjn_normalize_index(index, lst->count);
    if (index < 0) return NULL;
    return lst->items[index];
}

int zjn_list_len(const ZjnValue* list) {
    if (!list || list->type != ZVAL_LIST) return 0;
    return list->data.list_val->count;
}

ZjnValue* zjn_list_concat(const ZjnValue* a, const ZjnValue* b) {
    ZjnValue* result = zjn_val_list();
    ZjnList* la = a->data.list_val;
    ZjnList* lb = b->data.list_val;
    for (int i = 0; i < la->count; i++)
        zjn_list_append(result, zjn_val_copy(la->items[i]));
    for (int i = 0; i < lb->count; i++)
        zjn_list_append(result, zjn_val_copy(lb->items[i]));
    return result;
}

ZjnValue* zjn_list_repeat(const ZjnValue* list, long times) {
    ZjnValue* result = zjn_val_list();
    if (times <= 0) return result;
    ZjnList* src = list->data.list_val;
    for (long r = 0; r < times; r++)
        for (int i = 0; i < src->count; i++)
            zjn_list_append(result, zjn_val_copy(src->items[i]));
    return result;
}

/* ===================================================================
 *  字典操作（有序哈希表：O(1) 查找 + 插入序保持）
 * =================================================================== */

/* FNV-1a 字符串哈希（32 位） */
static unsigned long dict_hash(const char* key) {
    unsigned long h = 2166136261UL;
    while (*key) {
        h ^= (unsigned char)*key++;
        h *= 16777619UL;
    }
    return h;
}

/* 将全部条目重哈希进索引（扩容/删除后调用） */
static void dict_rehash(ZjnDict* d) {
    if (!d->index || d->index_cap <= 0) return;
    int mask = d->index_cap - 1;
    for (int i = 0; i < d->index_cap; i++) d->index[i] = -1;
    for (int e = 0; e < d->count; e++) {
        int slot = (int)(dict_hash(d->entries[e].key) & (unsigned long)mask);
        while (d->index[slot] >= 0) slot = (slot + 1) & mask;
        d->index[slot] = e;
    }
}

/* 哈希查找：返回条目指针；不存在返回 NULL */
static ZjnDictEntry* dict_find(const ZjnValue* dict, const char* key) {
    ZjnDict* d = dict->data.dict_val;
    if (!d->index || d->count == 0) return NULL;
    int mask = d->index_cap - 1;
    int slot = (int)(dict_hash(key) & (unsigned long)mask);
    for (int probe = 0; probe < d->index_cap; probe++) {
        int entry_idx = d->index[slot];
        if (entry_idx < 0) return NULL;
        if (strcmp(d->entries[entry_idx].key, key) == 0)
            return &d->entries[entry_idx];
        slot = (slot + 1) & mask;
    }
    return NULL;
}

/* 插入新条目到哈希索引（调用前须保证键不存在且 entries 已扩容） */
static void dict_index_insert(ZjnDict* d, int entry_idx) {
    int mask = d->index_cap - 1;
    int slot = (int)(dict_hash(d->entries[entry_idx].key) & (unsigned long)mask);
    while (d->index[slot] >= 0) slot = (slot + 1) & mask;
    d->index[slot] = entry_idx;
}

void zjn_dict_set(ZjnValue* dict, const char* key, ZjnValue* value) {
    if (!dict || dict->type != ZVAL_DICT) { if (value) zjn_val_free(value); return; }
    ZjnDict* d = dict->data.dict_val;
    ZjnDictEntry* existing = dict_find(dict, key);
    if (existing) {
        if (existing->value) zjn_val_free(existing->value);
        existing->value = value ? value : zjn_val_none();
        return;
    }
    /* entries 扩容 */
    if (d->count >= d->capacity) {
        int new_cap = d->capacity == 0 ? 8 : d->capacity * 2;
        ZjnDictEntry* ne = (ZjnDictEntry*)realloc(
            d->entries, new_cap * sizeof(ZjnDictEntry));
        if (!ne) { if (value) zjn_val_free(value); return; }
        d->entries = ne;
        d->capacity = new_cap;
    }
    /* 索引扩容（负载因子 > 2/3 时翻倍并整体重哈希） */
    if (d->index_cap == 0 || d->count * 3 >= d->index_cap * 2) {
        int new_ic = d->index_cap == 0 ? 8 : d->index_cap * 2;
        int* ni = (int*)realloc(d->index, new_ic * sizeof(int));
        if (!ni) { if (value) zjn_val_free(value); return; }
        d->index = ni;
        d->index_cap = new_ic;
        dict_rehash(d);
    }
    d->entries[d->count].key = strdup(key);
    d->entries[d->count].value = value ? value : zjn_val_none();
    d->count++;
    dict_index_insert(d, d->count - 1);
}

ZjnValue* zjn_dict_get(const ZjnValue* dict, const char* key) {
    if (!dict || dict->type != ZVAL_DICT) return NULL;
    ZjnDictEntry* e = dict_find(dict, key);
    return e ? e->value : NULL;
}

int zjn_dict_has(const ZjnValue* dict, const char* key) {
    if (!dict || dict->type != ZVAL_DICT) return 0;
    return dict_find(dict, key) != NULL;
}

void zjn_dict_remove(ZjnValue* dict, const char* key) {
    if (!dict || dict->type != ZVAL_DICT) return;
    ZjnDict* d = dict->data.dict_val;
    ZjnDictEntry* found = dict_find(dict, key);
    if (!found) return;
    int idx = (int)(found - d->entries);
    free(d->entries[idx].key);
    zjn_val_free(d->entries[idx].value);
    /* 有序数组前移（保持插入顺序） */
    for (int j = idx; j < d->count - 1; j++)
        d->entries[j] = d->entries[j + 1];
    d->count--;
    /* 下标整体变化，重哈希索引 */
    dict_rehash(d);
}

void zjn_dict_clear(ZjnValue* dict) {
    if (!dict || dict->type != ZVAL_DICT) return;
    ZjnDict* d = dict->data.dict_val;
    for (int i = 0; i < d->count; i++) {
        free(d->entries[i].key);
        zjn_val_free(d->entries[i].value);
    }
    d->count = 0;
    dict_rehash(d);
}

int zjn_dict_len(const ZjnValue* dict) {
    if (!dict || dict->type != ZVAL_DICT) return 0;
    return dict->data.dict_val->count;
}

/* ===================================================================
 *  字符串操作
 * =================================================================== */

int zjn_string_len(const ZjnValue* val) {
    if (!val || val->type != ZVAL_STRING) return 0;
    return (int)strlen(val->data.string_val);
}

/* 辅助：将任意值转为字符串（写入静态缓冲区，仅用于 concat 内部） */
static const char* val_to_str_for_concat(const ZjnValue* v, char* buf, int size) {
    if (!v) return "null";
    switch (v->type) {
        case ZVAL_STRING: return v->data.string_val;
        case ZVAL_NONE:   return "null";
        case ZVAL_BOOL:   return v->data.bool_val ? "yes" : "no";
        case ZVAL_INT:    snprintf(buf, size, "%ld", v->data.int_val); return buf;
        case ZVAL_FLOAT:  snprintf(buf, size, "%g", v->data.float_val); return buf;
        default:          return "<object>";
    }
}

ZjnValue* zjn_string_concat(const ZjnValue* a, const ZjnValue* b) {
    char buf_a[128], buf_b[128];
    const char* sa = val_to_str_for_concat(a, buf_a, sizeof(buf_a));
    const char* sb = val_to_str_for_concat(b, buf_b, sizeof(buf_b));
    size_t total = strlen(sa) + strlen(sb);
    char* result = (char*)malloc(total + 1);
    if (!result) return NULL;
    strcpy(result, sa);
    strcat(result, sb);
    ZjnValue* v = zjn_val_string(result);
    free(result);
    return v;
}

ZjnValue* zjn_string_repeat(const ZjnValue* str, long times) {
    const char* s = str->data.string_val;
    size_t slen = strlen(s);
    if (times <= 0) return zjn_val_string("");
    if ((size_t)times > (size_t)(ZUNJIN_MAX_SOURCE / (slen + 1))) {
        ZJN_THROW("字符串重复次数过大", 0, 0);
        return NULL;
    }
    size_t total = slen * (size_t)times;
    char* buf = (char*)malloc(total + 1);
    if (!buf) return NULL;
    for (long i = 0; i < times; i++)
        memcpy(buf + i * slen, s, slen);
    buf[total] = '\0';
    ZjnValue* v = zjn_val_string(buf);
    free(buf);
    return v;
}

ZjnValue* zjn_string_sub(const ZjnValue* str, int start, int len) {
    const char* s = str->data.string_val;
    int slen = (int)strlen(s);
    if (start < 0) start = 0;
    if (start > slen) start = slen;
    if (len < 0) len = 0;
    if (start + len > slen) len = slen - start;
    return zjn_val_string_from(s + start, len);
}

/* ===================================================================
 *  序列统一操作
 * =================================================================== */

/* 索引归一化：-1 表示最后一个，依此类推；无效返回 -1 */
int zjn_normalize_index(int index, int len) {
    if (index < 0) index += len;
    if (index < 0 || index >= len) return -1;
    return index;
}

ZjnValue* zjn_val_len(const ZjnValue* val) {
    if (!val) return zjn_val_int(0);
    switch (val->type) {
        case ZVAL_LIST:   return zjn_val_int(val->data.list_val->count);
        case ZVAL_STRING: return zjn_val_int((long)strlen(val->data.string_val));
        case ZVAL_DICT:   return zjn_val_int(val->data.dict_val->count);
        default:          return zjn_val_int(0);
    }
}

int zjn_val_contains(const ZjnValue* container, const ZjnValue* item) {
    if (!container) return 0;
    switch (container->type) {
        case ZVAL_LIST: {
            ZjnList* lst = container->data.list_val;
            for (int i = 0; i < lst->count; i++)
                if (zjn_val_equal(lst->items[i], item)) return 1;
            return 0;
        }
        case ZVAL_STRING: {
            if (!item || item->type != ZVAL_STRING) return 0;
            return strstr(container->data.string_val, item->data.string_val) != NULL;
        }
        case ZVAL_DICT: {
            if (!item || item->type != ZVAL_STRING) return 0;
            return zjn_dict_has(container, item->data.string_val);
        }
        default:
            return 0;
    }
}

/* 统一索引：列表返回元素引用副本；字符串返回单字符；字典仅支持字符串键 */
ZjnValue* zjn_val_index(const ZjnValue* container, int index) {
    if (!container) return zjn_val_none();
    switch (container->type) {
        case ZVAL_LIST: {
            ZjnList* lst = container->data.list_val;
            int i = zjn_normalize_index(index, lst->count);
            if (i < 0) return NULL;
            return zjn_val_copy(lst->items[i]);
        }
        case ZVAL_STRING: {
            const char* s = container->data.string_val;
            int slen = (int)strlen(s);
            int i = zjn_normalize_index(index, slen);
            if (i < 0) return NULL;
            return zjn_val_string_from(s + i, 1);
        }
        case ZVAL_DICT:
            return NULL; /* 字典索引使用键名（字符串），由 VM 处理 */
        default:
            return NULL;
    }
}

/* 切片：start/stop 已归一化（负数 +len，None 处理由 VM 完成）
 * 返回新容器；step 为 0 视为 1 */
ZjnValue* zjn_val_slice(const ZjnValue* container, int start, int stop, int step) {
    if (!container) return zjn_val_none();
    if (step == 0) step = 1;
    switch (container->type) {
        case ZVAL_LIST: {
            ZjnList* src = container->data.list_val;
            ZjnValue* result = zjn_val_list();
            if (step > 0) {
                for (int i = start; i < stop && i < src->count; i += step) {
                    if (i < 0) continue;
                    zjn_list_append(result, zjn_val_copy(src->items[i]));
                }
            } else {
                for (int i = start; i > stop && i >= 0; i += step) {
                    if (i >= src->count) continue;
                    zjn_list_append(result, zjn_val_copy(src->items[i]));
                }
            }
            return result;
        }
        case ZVAL_STRING: {
            const char* s = container->data.string_val;
            int slen = (int)strlen(s);
            char* buf = NULL;
            int count = 0;
            if (step > 0) {
                for (int i = start; i < stop && i < slen; i += step) {
                    if (i < 0) continue;
                    buf = realloc(buf, count + 2);
                    buf[count++] = s[i];
                }
            } else {
                for (int i = start; i > stop && i >= 0; i += step) {
                    if (i >= slen) continue;
                    buf = realloc(buf, count + 2);
                    buf[count++] = s[i];
                }
            }
            if (!buf) return zjn_val_string("");
            buf[count] = '\0';
            ZjnValue* v = zjn_val_string(buf);
            free(buf);
            return v;
        }
        default:
            return zjn_val_none();
    }
}

/* ===================================================================
 *  值转换
 * =================================================================== */

int zjn_val_to_bool(const ZjnValue* val) {
    if (!val) return 0;
    switch (val->type) {
        case ZVAL_NONE:   return 0;
        case ZVAL_BOOL:   return val->data.bool_val;
        case ZVAL_INT:    return val->data.int_val != 0;
        case ZVAL_FLOAT:  return val->data.float_val != 0.0;
        case ZVAL_STRING: return strlen(val->data.string_val) > 0;
        case ZVAL_LIST:   return val->data.list_val->count > 0;
        case ZVAL_DICT:   return val->data.dict_val->count > 0;
        default:          return 1;
    }
}

long zjn_val_to_int(const ZjnValue* val) {
    if (!val) return 0;
    switch (val->type) {
        case ZVAL_INT:    return val->data.int_val;
        case ZVAL_FLOAT:  return (long)val->data.float_val;
        case ZVAL_BOOL:   return val->data.bool_val ? 1 : 0;
        case ZVAL_STRING: return atol(val->data.string_val);
        default:          return 0;
    }
}

double zjn_val_to_float(const ZjnValue* val) {
    if (!val) return 0.0;
    switch (val->type) {
        case ZVAL_FLOAT:  return val->data.float_val;
        case ZVAL_INT:    return (double)val->data.int_val;
        case ZVAL_BOOL:   return val->data.bool_val ? 1.0 : 0.0;
        case ZVAL_STRING: return atof(val->data.string_val);
        default:          return 0.0;
    }
}

const char* zjn_val_to_string(const ZjnValue* val) {
    if (!val) return "null";
    if (val->type == ZVAL_STRING) return val->data.string_val;
    return "";
}

int zjn_val_is_truthy(const ZjnValue* val) {
    return zjn_val_to_bool(val);
}

/* 比较：数值按数值、字符串按字典序、布尔按大小、其他按类型 */
int zjn_val_compare(const ZjnValue* a, const ZjnValue* b) {
    if (!a || !b) return a == b ? 0 : (a ? 1 : -1);
    if (a->type == b->type) {
        switch (a->type) {
            case ZVAL_NONE:   return 0;
            case ZVAL_BOOL:   return (a->data.bool_val > b->data.bool_val) -
                                      (a->data.bool_val < b->data.bool_val);
            case ZVAL_INT:
                return (a->data.int_val > b->data.int_val) -
                       (a->data.int_val < b->data.int_val);
            case ZVAL_FLOAT:
                return (a->data.float_val > b->data.float_val) -
                       (a->data.float_val < b->data.float_val);
            case ZVAL_STRING: {
                int r = strcmp(a->data.string_val, b->data.string_val);
                return r > 0 ? 1 : (r < 0 ? -1 : 0);
            }
            default:
                return 0;
        }
    }
    /* 数字与数字、数字与布尔按数值比较 */
    if (ZJN_IS_NUMBER(a) && (ZJN_IS_NUMBER(b) || ZJN_IS_BOOL(b))) {
        double x = zjn_val_to_float(a), y = zjn_val_to_float(b);
        return x > y ? 1 : (x < y ? -1 : 0);
    }
    if (ZJN_IS_BOOL(a) && ZJN_IS_NUMBER(b)) {
        double x = zjn_val_to_float(a), y = zjn_val_to_float(b);
        return x > y ? 1 : (x < y ? -1 : 0);
    }
    return 0;
}

int zjn_val_equal(const ZjnValue* a, const ZjnValue* b) {
    if (!a || !b) return a == b;
    if (a->type != b->type) {
        /* 数字/布尔类型间比较 */
        if ((ZJN_IS_NUMBER(a) || ZJN_IS_BOOL(a)) &&
            (ZJN_IS_NUMBER(b) || ZJN_IS_BOOL(b))) {
            return zjn_val_to_float(a) == zjn_val_to_float(b);
        }
        return 0;
    }
    switch (a->type) {
        case ZVAL_NONE:   return 1;
        case ZVAL_BOOL:   return a->data.bool_val == b->data.bool_val;
        case ZVAL_INT:    return a->data.int_val == b->data.int_val;
        case ZVAL_FLOAT:  return a->data.float_val == b->data.float_val;
        case ZVAL_STRING: return strcmp(a->data.string_val, b->data.string_val) == 0;
        case ZVAL_LIST: {
            ZjnList* la = a->data.list_val;
            ZjnList* lb = b->data.list_val;
            if (la->count != lb->count) return 0;
            for (int i = 0; i < la->count; i++)
                if (!zjn_val_equal(la->items[i], lb->items[i])) return 0;
            return 1;
        }
        default:          return a == b;
    }
}
