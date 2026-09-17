<!--
SPDX-License-Identifier: LGPL-3.0-only OR Commercial
-->

# 构建指南

## 依赖

- 任意支持 **C11/C17** 的 C 编译器（gcc / clang / MSVC）
- Windows 平台：MinGW-w64 / MSYS2 / Visual Studio
- 链接库：`libm`（数学库）

---

## 方法一：Makefile（跨平台推荐）

### 可用命令

```bash
make             # 编译生成 zunjin（Windows 上为 zunjin.exe）
make run         # 编译并运行 Samples/demo.z
make test        # 运行测试
make debug       # 调试模式（-O0 -g -DDEBUG）
make clean       # 清理构建产物
make rebuild     # 清理后重新编译
```

### Linux / macOS

```bash
make
./zunjin Samples/demo.z
```

### Windows（MSYS2 / MinGW）

```bash
make
zunjin.exe Samples/demo.z
```

---

## 方法二：Windows 批处理

在 `BuildScripts` 目录下提供了 Windows 批处理脚本：

```bat
cd BuildScripts
build.bat       :: 编译
clean.bat       :: 清理
```

> 编译产物输出到项目根目录。

---

## 方法三：直接调用编译器

```bash
gcc -Wall -Wextra -std=c11 -O2 -IHeaders \
    Entry/main.c Syntax/lexer/lexer.c Syntax/parser_core.c \
    Syntax/parser_stmt.c Syntax/parser_expr.c Core/ast.c \
    Core/vm_core.c Core/vm_stmt.c Core/vm_expr.c Core/vm_module.c \
    Core/env.c Core/output.c Runtime/object.c Builtins/builtins.c \
    -o zunjin -lm
```

---

## Makefile 详解

### 编译标志

| 标志 | 说明 |
|------|------|
| `-Wall -Wextra` | 启用所有警告 |
| `-std=gnu11` | C11 标准（GNU 扩展） |
| `-O2 -g` | 优化级别 2 + 调试信息 |
| `-D_WIN32` | Windows 平台宏（自动检测） |

### 目录结构

```
zunjin/
├── Headers/          # 头文件
├── Entry/            # 入口
├── Syntax/           # 词法/语法
├── Core/             # 虚拟机核心
├── Runtime/          # 对象系统
├── Builtins/         # 内置函数
├── Platform/         # 平台相关
├── obj/              # Linux/macOS 编译中间文件
├── obj_win/          # Windows 编译中间文件
└── zunjin.exe        # 编译产物
```

### 调试模式

```bash
make debug       # 相当于 make DEBUG=1
```

调试模式使用 `-O0 -g -DDEBUG` 编译，关闭优化，方便在 gdb 或 lldb 中调试。

---

## 平台支持

| 平台 | 编译器 | 状态 |
|------|--------|------|
| Linux | gcc / clang | ✅ 已测试 |
| macOS | gcc / clang | ✅ 已测试 |
| Windows (MinGW) | gcc | ✅ 已测试 |
| Windows (MSVC) | cl | ⚠️ 需调整编译选项 |

---

## 许可证

zunjin 采用 **双许可证** 模式，详情见 [LICENSE](LICENSE) 和 [CLA.md](CLA.md)。