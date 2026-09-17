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
 * zunjin 语言 - 词法分析器实现
 * 完全独立重新实现
 * 将 .z 源码拆分为 Token 流，支持缩进、注释、字符串、数字
 *
 * 版本 2.0 新增：
 *  - 三引号字符串 """...""" / '''...'''（支持多行）
 *  - 十六/八/二进制字面量 0x/0o/0b、下划线数字、科学计数法
 *  - 幂/位运算符 ** << >> & | ^ ~ 与复合赋值 += -= *= /= //= %= **=
 *  - 大括号 {} 与异常关键字 attempt/seize/settle/fling
 *  - 未闭合字符串/非法字符改为显式报错（TOK_ERROR + 消息）
 * ========================================================================== */

#include "lexer.h"
#include <string.h>
#include <ctype.h>
#include <stdio.h>

/* ---------- 关键字查找表（按名称字典序排列，供二分查找） ---------- */
typedef struct {
    const char* word;
    ZjnTokenType type;
} KeywordEntry;

static KeywordEntry keywords[] = {
    /* 仅 zunjin 自创关键字，无兼容别名 */
    {"attempt",    TOK_TRY},
    {"both",       TOK_AND},
    {"either",     TOK_OR},
    {"fling",      TOK_RAISE},
    {"foreach",    TOK_FOR},
    {"give",       TOK_RETURN},
    {"loop",       TOK_WHILE},
    {"negate",     TOK_NOT},
    {"next",       TOK_CONTINUE},
    {"nil",        TOK_NONE},
    {"no",         TOK_FALSE},
    {"of",         TOK_IN},
    {"otherwise",  TOK_ELSE},
    {"proc",       TOK_FUNC},
    {"seize",      TOK_CATCH},
    {"settle",     TOK_FINALLY},
    {"skip",       TOK_PASS},
    {"stop",       TOK_BREAK},
    {"use",        TOK_IMPORT},
    {"when",       TOK_IF},
    {"whenelse",   TOK_ELIF},
    {"yes",        TOK_TRUE},
};

#define KEYWORD_COUNT (int)(sizeof(keywords) / sizeof(keywords[0]))

/* 二分查找关键字（O(log n) 替代线性扫描） */
static const KeywordEntry* keyword_lookup(const char* word) {
    int lo = 0, hi = KEYWORD_COUNT - 1;
    while (lo <= hi) {
        int mid = (lo + hi) / 2;
        int cmp = strcmp(word, keywords[mid].word);
        if (cmp == 0) return &keywords[mid];
        if (cmp < 0) hi = mid - 1;
        else lo = mid + 1;
    }
    return NULL;
}

/* ---------- Token 类型名 ---------- */
const char* zjn_token_type_name(ZjnTokenType type) {
    static const char* names[] = {
        "NUMBER", "STRING", "IDENTIFIER",
        "WHEN", "WHENELSE", "OTHERWISE", "LOOP", "FOREACH", "OF",
        "PROC", "GIVE", "BOTH", "EITHER", "NEGATE",
        "YES", "NO", "NIL",
        "STOP", "NEXT", "USE", "SKIP",
        "ATTEMPT", "SEIZE", "SETTLE", "FLING",
        "PLUS", "MINUS", "STAR", "SLASH", "DBL_SLASH", "PERCENT", "POW",
        "SHL", "SHR", "BAND", "BOR", "BXOR", "BNOT",
        "ASSIGN", "PLUS_ASSIGN", "MINUS_ASSIGN", "STAR_ASSIGN",
        "SLASH_ASSIGN", "DBL_SLASH_ASSIGN", "PERCENT_ASSIGN", "POW_ASSIGN",
        "EQ", "NEQ", "LT", "GT", "LE", "GE", "NOT_IN",
        "LPAREN", "RPAREN", "LBRACKET", "RBRACKET",
        "LBRACE", "RBRACE", "COMMA", "COLON", "DOT",
        "NEWLINE", "INDENT", "DEDENT",
        "EOF", "ERROR",
    };
    if (type >= 0 && type < sizeof(names)/sizeof(names[0]))
        return names[type];
    return "UNKNOWN";
}

/* ---------- Token 创建辅助 ---------- */
/* Token 空闲链表：解析器每次只持有少数 token，释放后结构体复用，
 * 避免大型源文件解析时的大量 malloc/free（等价小对象 free-list 思路） */
#define TOKEN_FREE_MAX 8192
static ZjnToken* g_token_free = NULL;
static int g_token_free_count = 0;

static ZjnToken* token_alloc(void) {
    ZjnToken* tok = NULL;
    if (g_token_free) {
        tok = g_token_free;
        g_token_free = (ZjnToken*)(void*)tok->data.ident_val; /* 借指针字段作 next */
        g_token_free_count--;
    } else {
        tok = ZUNJIN_ALLOC(ZjnToken);
    }
    if (!tok) return NULL;
    memset(tok, 0, sizeof(ZjnToken));
    return tok;
}

static void token_recycle(ZjnToken* tok) {
    if (!tok) return;
    if (g_token_free_count < TOKEN_FREE_MAX) {
        tok->data.ident_val = (char*)(void*)g_token_free;
        g_token_free = tok;
        g_token_free_count++;
    } else {
        free(tok);
    }
}

static ZjnToken* make_token(ZjnTokenType type, int line, int col) {
    ZjnToken* tok = token_alloc();
    if (!tok) return NULL;
    tok->type = type;
    tok->line = line;
    tok->column = col;
    return tok;
}

static ZjnToken* make_num_token(long ival, double fval, int is_float,
                                 int line, int col) {
    ZjnToken* tok = make_token(TOK_NUMBER, line, col);
    if (!tok) return NULL;
    tok->is_float = is_float;
    if (is_float) {
        tok->data.float_val = fval;
    } else {
        tok->data.int_val = ival;
    }
    return tok;
}

static ZjnToken* make_str_token(const char* s, int line, int col) {
    ZjnToken* tok = make_token(TOK_STRING, line, col);
    if (!tok) return NULL;
    tok->data.string_val = strdup(s);
    return tok;
}

static ZjnToken* make_id_token(const char* s, ZjnTokenType type,
                                int line, int col) {
    ZjnToken* tok = make_token(type, line, col);
    if (!tok) return NULL;
    tok->data.ident_val = strdup(s);
    return tok;
}

/* 错误 Token：data.ident_val 携带错误消息 */
static ZjnToken* make_error_token(const char* msg, int line, int col) {
    return make_id_token(msg, TOK_ERROR, line, col);
}

/* ---------- 释放 Token（结构体进空闲链表复用） ---------- */
void zjn_token_free(ZjnToken* token) {
    if (!token) return;
    if (token->type == TOK_STRING && token->data.string_val) {
        free(token->data.string_val);
    }
    if (token->type == TOK_IDENTIFIER && token->data.ident_val) {
        free(token->data.ident_val);
    }
    if (token->type == TOK_ERROR && token->data.ident_val) {
        free(token->data.ident_val);
    }
    if ((token->type >= TOK_IF && token->type <= TOK_RAISE) &&
         token->data.ident_val) {
        free(token->data.ident_val);
    }
    token_recycle(token);
}

/* ===================================================================
 *  词法分析器实现
 * =================================================================== */

ZjnLexer* zjn_lexer_create(const char* source) {
    ZjnLexer* lexer = ZUNJIN_ALLOC(ZjnLexer);
    if (!lexer) return NULL;
    lexer->source = source;
    lexer->source_len = (int)strlen(source);
    lexer->pos = 0;
    lexer->line = 1;
    lexer->col = 1;
    lexer->current_indent = 0;
    lexer->pending_dedents = 0;
    lexer->bracket_depth = 0;

    /* 缩进栈 */
    lexer->indent_capacity = 64;
    lexer->indent_stack = ZUNJIN_ALLOC_N(int, lexer->indent_capacity);
    if (!lexer->indent_stack) { free(lexer); return NULL; }
    lexer->indent_stack[0] = 0;
    lexer->indent_top = 0;

    memset(&lexer->error, 0, sizeof(ZjnError));
    return lexer;
}

void zjn_lexer_free(ZjnLexer* lexer) {
    if (!lexer) return;
    if (lexer->indent_stack) free(lexer->indent_stack);
    free(lexer);
}

/* ---------- 辅助函数 ---------- */
static int peek(ZjnLexer* lexer, int offset) {
    int p = lexer->pos + offset;
    if (p < lexer->source_len) return (unsigned char)lexer->source[p];
    return -1;
}

static int advance(ZjnLexer* lexer) {
    if (lexer->pos < lexer->source_len) {
        int ch = (unsigned char)lexer->source[lexer->pos++];
        lexer->col++;
        return ch;
    }
    return -1;
}

static void skip_line(ZjnLexer* lexer) {
    int ch;
    while ((ch = peek(lexer, 0)) != -1) {
        if (ch == '\n') break;
        advance(lexer);
    }
}

/* ---------- 读取数字（整数/浮点/进制/下划线/科学计数） ---------- */
static ZjnToken* read_number(ZjnLexer* lexer) {
    int line = lexer->line;
    int col = lexer->col;
    char buf[128];
    int idx = 0;
    int is_float = 0;

    /* 进制前缀 */
    if (peek(lexer, 0) == '0') {
        int p1 = peek(lexer, 1);
        if (p1 == 'x' || p1 == 'X') {
            buf[idx++] = (char)advance(lexer); /* 0 */
            buf[idx++] = (char)advance(lexer); /* x */
            while (idx < 127) {
                int ch = peek(lexer, 0);
                if (isxdigit(ch)) {
                    buf[idx++] = (char)advance(lexer);
                } else if (ch == '_') {
                    advance(lexer); /* 下划线分隔符 */
                } else {
                    break;
                }
            }
            buf[idx] = '\0';
            return make_num_token(strtol(buf, NULL, 16), 0.0, 0, line, col);
        }
        if (p1 == 'o' || p1 == 'O') {
            buf[idx++] = (char)advance(lexer);
            buf[idx++] = (char)advance(lexer);
            while (idx < 127) {
                int ch = peek(lexer, 0);
                if (ch >= '0' && ch <= '7') {
                    buf[idx++] = (char)advance(lexer);
                } else if (ch == '_') {
                    advance(lexer);
                } else {
                    break;
                }
            }
            buf[idx] = '\0';
            /* strtol 不认识 0o 前缀（zunjin 语法），跳过前缀再解析 */
            return make_num_token(strtol(buf + 2, NULL, 8), 0.0, 0, line, col);
        }
        if (p1 == 'b' || p1 == 'B') {
            buf[idx++] = (char)advance(lexer);
            buf[idx++] = (char)advance(lexer);
            while (idx < 127) {
                int ch = peek(lexer, 0);
                if (ch == '0' || ch == '1') {
                    buf[idx++] = (char)advance(lexer);
                } else if (ch == '_') {
                    advance(lexer);
                } else {
                    break;
                }
            }
            buf[idx] = '\0';
            /* strtol 不认识 0b 前缀（zunjin 语法），跳过前缀再解析 */
            return make_num_token(strtol(buf + 2, NULL, 2), 0.0, 0, line, col);
        }
    }

    /* 十进制（含下划线、小数点、指数） */
    while (idx < 127) {
        int ch = peek(lexer, 0);
        if (ch >= '0' && ch <= '9') {
            buf[idx++] = (char)advance(lexer);
        } else if (ch == '_') {
            advance(lexer); /* 下划线分隔符 */
        } else if (ch == '.' && !is_float) {
            is_float = 1;
            buf[idx++] = (char)advance(lexer);
        } else if ((ch == 'e' || ch == 'E') && idx > 0) {
            is_float = 1;
            buf[idx++] = (char)advance(lexer);
            /* 指数符号 */
            if (peek(lexer, 0) == '+' || peek(lexer, 0) == '-') {
                buf[idx++] = (char)advance(lexer);
            }
        } else {
            break;
        }
    }
    buf[idx] = '\0';

    if (is_float) {
        return make_num_token(0, strtod(buf, NULL), 1, line, col);
    } else {
        return make_num_token(strtol(buf, NULL, 10), 0.0, 0, line, col);
    }
}

/* ---------- 读取字符串 ----------
 * triple = 1 表示三引号（允许跨行），否则单行字符串
 * 未闭合时报错（TOK_ERROR + 消息） */
static ZjnToken* read_string(ZjnLexer* lexer, int quote, int triple) {
    int line = lexer->line;
    int col = lexer->col;
    char buf[ZUNJIN_MAX_LINE * 4];
    int idx = 0;
    int closed = 0;

    while (idx < (int)sizeof(buf) - 4) {
        int ch = advance(lexer);
        if (ch == -1) break;
        if (ch == '\\') {
            int next = advance(lexer);
            if (next == -1) break;
            switch (next) {
                case 'n': buf[idx++] = '\n'; break;
                case 't': buf[idx++] = '\t'; break;
                case 'r': buf[idx++] = '\r'; break;
                case '0': buf[idx++] = '\0'; break;
                case '\\': buf[idx++] = '\\'; break;
                case '"': buf[idx++] = '"'; break;
                case '\'': buf[idx++] = '\''; break;
                default: buf[idx++] = (char)next; break;
            }
        } else if (ch == quote) {
            if (triple) {
                if (peek(lexer, 0) == quote && peek(lexer, 1) == quote) {
                    advance(lexer);
                    advance(lexer);
                    closed = 1;
                    break;
                }
                buf[idx++] = (char)ch; /* 三引号内单个引号字符 */
            } else {
                closed = 1;
                break;
            }
        } else if (ch == '\n') {
            lexer->line++;
            lexer->col = 1;
            if (!triple) {
                /* 单行字符串内出现换行视为未闭合 */
                return make_error_token("字符串未闭合（缺少结束引号）", line, col);
            }
            buf[idx++] = '\n';
        } else {
            buf[idx++] = (char)ch;
        }
    }
    buf[idx] = '\0';

    if (!closed) {
        return make_error_token("字符串未闭合（缺少结束引号）", line, col);
    }
    return make_str_token(buf, line, col);
}

/* ---------- 读取标识符/关键字 ---------- */
static ZjnToken* read_identifier(ZjnLexer* lexer) {
    int line = lexer->line;
    int col = lexer->col;
    char buf[128];
    int idx = 0;

    while (idx < 127) {
        int ch = peek(lexer, 0);
        if (isalnum(ch) || ch == '_') {
            buf[idx++] = (char)advance(lexer);
        } else {
            break;
        }
    }
    buf[idx] = '\0';

    /* 查关键字表（二分查找，O(log n)） */
    const KeywordEntry* kw = keyword_lookup(buf);
    if (kw) {
        return make_id_token(buf, kw->type, line, col);
    }
    return make_id_token(buf, TOK_IDENTIFIER, line, col);
}

/* ---------- 获取下一个 Token ---------- */
ZjnToken* zjn_lexer_next(ZjnLexer* lexer) {
    while (lexer->pos < lexer->source_len) {
        /* 先吐出挂起的 DEDENT：一次缩进回退多层时逐次返回
         * （修复原实现仅在行首返回一个 DEDENT 导致多层回退丢失的问题） */
        if (lexer->pending_dedents > 0) {
            lexer->pending_dedents--;
            return make_token(TOK_DEDENT, lexer->line, 1);
        }

        int ch = peek(lexer, 0);

        /* 处理可能的缩进（行首）；括号深度 > 0 时不处理（zunjin 语义） */
        if (lexer->col == 1 && lexer->bracket_depth == 0) {
            int indent = 0;
            while (peek(lexer, 0) == ' ') {
                advance(lexer);
                indent++;
            }
            lexer->current_indent = indent;

            /* 跳过空行和注释行 */
            int next = peek(lexer, 0);
            if (next == '\n' || next == '\r' || next == '#') {
                if (next == '#') skip_line(lexer);
                /* 跳行 */
                if (peek(lexer, 0) == '\r') { advance(lexer); }
                if (peek(lexer, 0) == '\n') { advance(lexer); }
                lexer->line++;
                lexer->col = 1;
                continue;
            }

            /* 处理缩进 */
            if (indent > lexer->indent_stack[lexer->indent_top]) {
                lexer->indent_stack[++lexer->indent_top] = indent;
                return make_token(TOK_INDENT, lexer->line, 1);
            }
            while (indent < lexer->indent_stack[lexer->indent_top]) {
                lexer->indent_top--;
                lexer->pending_dedents++;
            }
            if (lexer->pending_dedents > 0) {
                continue;   /* 回到顶部逐次吐出 DEDENT */
            }

            /* 缩进匹配且缩进 > 0 时，重新进入循环以更新 ch 变量，避免重复消费空白 */
            if (indent > 0) {
                continue;
            }
        }

        /* 跳过空白 */
        if (ch == ' ' || ch == '\t') {
            advance(lexer);
            continue;
        }

        /* 注释 */
        if (ch == '#') {
            skip_line(lexer);
            continue;
        }

        /* 换行 */
        if (ch == '\n' || ch == '\r') {
            int line = lexer->line;
            if (ch == '\r') advance(lexer);
            if (peek(lexer, 0) == '\n') advance(lexer);
            lexer->line++;
            lexer->col = 1;
            /* 括号内换行不产生 NEWLINE */
            if (lexer->bracket_depth > 0) continue;
            return make_token(TOK_NEWLINE, line, 1);
        }

        /* 数字 */
        if (ch >= '0' && ch <= '9') {
            return read_number(lexer);
        }

        /* 字符串（含三引号） */
        if (ch == '"' || ch == '\'') {
            int triple = (peek(lexer, 1) == ch) && (peek(lexer, 2) == ch);
            if (triple) {
                advance(lexer);
                advance(lexer);
                advance(lexer);
            } else {
                advance(lexer);
            }
            return read_string(lexer, ch, triple);
        }

        /* 标识符 */
        if (isalpha(ch) || ch == '_') {
            return read_identifier(lexer);
        }

        /* 运算符和分隔符 */
        switch (ch) {
            case '+':
                advance(lexer);
                if (peek(lexer, 0) == '=') { advance(lexer); return make_token(TOK_PLUS_ASSIGN, lexer->line, lexer->col-2); }
                return make_token(TOK_PLUS, lexer->line, lexer->col-1);
            case '-':
                advance(lexer);
                if (peek(lexer, 0) == '=') { advance(lexer); return make_token(TOK_MINUS_ASSIGN, lexer->line, lexer->col-2); }
                return make_token(TOK_MINUS, lexer->line, lexer->col-1);
            case '*':
                advance(lexer);
                if (peek(lexer, 0) == '*') {
                    advance(lexer);
                    if (peek(lexer, 0) == '=') { advance(lexer); return make_token(TOK_POW_ASSIGN, lexer->line, lexer->col-3); }
                    return make_token(TOK_POW, lexer->line, lexer->col-2);
                }
                if (peek(lexer, 0) == '=') { advance(lexer); return make_token(TOK_STAR_ASSIGN, lexer->line, lexer->col-2); }
                return make_token(TOK_STAR, lexer->line, lexer->col-1);
            case '%':
                advance(lexer);
                if (peek(lexer, 0) == '=') { advance(lexer); return make_token(TOK_PERCENT_ASSIGN, lexer->line, lexer->col-2); }
                return make_token(TOK_PERCENT, lexer->line, lexer->col-1);
            case '(': advance(lexer); lexer->bracket_depth++; return make_token(TOK_LPAREN, lexer->line, lexer->col-1);
            case ')': advance(lexer); if (lexer->bracket_depth > 0) lexer->bracket_depth--; return make_token(TOK_RPAREN, lexer->line, lexer->col-1);
            case ',': advance(lexer); return make_token(TOK_COMMA, lexer->line, lexer->col-1);
            case ':': advance(lexer); return make_token(TOK_COLON, lexer->line, lexer->col-1);
            /* 属性访问点号：仅当后随字母/下划线时为 DOT（避免吞掉浮点小数部分） */
            case '.':
                advance(lexer);
                if (isalpha(peek(lexer, 0)) || peek(lexer, 0) == '_')
                    return make_token(TOK_DOT, lexer->line, lexer->col-1);
                return make_error_token("非法字符 '.'", lexer->line, lexer->col-1);
            case '[': advance(lexer); lexer->bracket_depth++; return make_token(TOK_LBRACKET, lexer->line, lexer->col-1);
            case ']': advance(lexer); if (lexer->bracket_depth > 0) lexer->bracket_depth--; return make_token(TOK_RBRACKET, lexer->line, lexer->col-1);
            case '{': advance(lexer); lexer->bracket_depth++; return make_token(TOK_LBRACE, lexer->line, lexer->col-1);
            case '}': advance(lexer); if (lexer->bracket_depth > 0) lexer->bracket_depth--; return make_token(TOK_RBRACE, lexer->line, lexer->col-1);
            case '=':
                advance(lexer);
                if (peek(lexer, 0) == '=') { advance(lexer); return make_token(TOK_EQ, lexer->line, lexer->col-2); }
                return make_token(TOK_ASSIGN, lexer->line, lexer->col-1);
            case '!':
                advance(lexer);
                if (peek(lexer, 0) == '=') { advance(lexer); return make_token(TOK_NEQ, lexer->line, lexer->col-2); }
                return make_error_token("非法字符 '!'", lexer->line, lexer->col-1);
            case '<':
                advance(lexer);
                if (peek(lexer, 0) == '<') { advance(lexer); return make_token(TOK_SHL, lexer->line, lexer->col-2); }
                if (peek(lexer, 0) == '=') { advance(lexer); return make_token(TOK_LE, lexer->line, lexer->col-2); }
                return make_token(TOK_LT, lexer->line, lexer->col-1);
            case '>':
                advance(lexer);
                if (peek(lexer, 0) == '>') { advance(lexer); return make_token(TOK_SHR, lexer->line, lexer->col-2); }
                if (peek(lexer, 0) == '=') { advance(lexer); return make_token(TOK_GE, lexer->line, lexer->col-2); }
                return make_token(TOK_GT, lexer->line, lexer->col-1);
            case '&':
                advance(lexer);
                if (peek(lexer, 0) == '=') { advance(lexer); return make_error_token("暂不支持 '&='", lexer->line, lexer->col-2); }
                return make_token(TOK_BAND, lexer->line, lexer->col-1);
            case '|':
                advance(lexer);
                if (peek(lexer, 0) == '=') { advance(lexer); return make_error_token("暂不支持 '|='", lexer->line, lexer->col-2); }
                return make_token(TOK_BOR, lexer->line, lexer->col-1);
            case '^':
                advance(lexer);
                if (peek(lexer, 0) == '=') { advance(lexer); return make_error_token("暂不支持 '^='", lexer->line, lexer->col-2); }
                return make_token(TOK_BXOR, lexer->line, lexer->col-1);
            case '~':
                advance(lexer);
                return make_token(TOK_BNOT, lexer->line, lexer->col-1);
            case '/':
                advance(lexer);
                if (peek(lexer, 0) == '/') {
                    advance(lexer);
                    if (peek(lexer, 0) == '=') { advance(lexer); return make_token(TOK_DBL_SLASH_ASSIGN, lexer->line, lexer->col-3); }
                    return make_token(TOK_DBL_SLASH, lexer->line, lexer->col-2);
                }
                if (peek(lexer, 0) == '=') { advance(lexer); return make_token(TOK_SLASH_ASSIGN, lexer->line, lexer->col-2); }
                return make_token(TOK_SLASH, lexer->line, lexer->col-1);
            default:
                advance(lexer);
                return make_error_token("非法字符", lexer->line, lexer->col-1);
        }
    }

    /* 文件末尾 - 处理未闭合的 DEDENT */
    while (lexer->indent_top > 0) {
        lexer->indent_top--;
        return make_token(TOK_DEDENT, lexer->line, 1);
    }

    return make_token(TOK_EOF, lexer->line, 1);
}
