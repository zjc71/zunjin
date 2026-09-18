# zunjin 编程语言

> 一门完全独立实现的脚本编程语言，拥有自创的关键字体系，不兼容任何其他语言关键字。

[![License: LGPL v3](https://img.shields.io/badge/License-LGPL%20v3-blue.svg)](https://www.gnu.org/licenses/lgpl-3.0)
![Platform](https://img.shields.io/badge/Platform-Windows%20%7C%20Linux%20%7C%20macOS-lightgrey.svg)
![Language](https://img.shields.io/badge/Language-C11-orange.svg)

- **当前版本**：0.0.1
- **文件扩展名**：`.z`
- **源码编码**：UTF-8（支持中文）
- **运行方式**：`zunjin.exe 脚本.z`，无参数进入交互模式（REPL）

---

## 目录

- [简介](#简介)
- [核心特性](#核心特性)
- [目录结构](#目录结构)
- [构建](#构建)
- [快速上手](#快速上手)
- [关键字总表](#关键字总表)
- [数据类型](#数据类型)
- [运算符](#运算符)
- [控制流](#控制流)
- [函数](#函数)
- [异常处理](#异常处理)
- [模块系统](#模块系统)
- [内置函数](#内置函数)
- [GUI 窗口编程](#gui-窗口编程)
- [架构设计](#架构设计)
- [许可证](#许可证)

---

## 简介

zunjin 是一门**完全独立**设计的脚本编程语言，从词法分析器、语法解析器、AST 管理、虚拟机执行器到对象系统、内置函数全部自研，不复制任何其他语言源码。

- 关键字体系完全自创（`when` / `loop` / `proc` / `give` / `both` / `either` 等），**不与主流脚本语言关键字重名**。
- 缩进式语法，无花括号；支持跨行字面量与尾随逗号。
- 引用计数 + 空闲链表内存管理，避免悬垂指针与双重释放。
- 有序哈希字典（插入序 + O(1) 查找），紧凑字典实现思路。
- 模块系统（`use` 语句）支持模块缓存与命名空间隔离。
- Windows 平台内置 GUI 内置函数，可编写窗口程序。

> 设计原则：可借鉴主流脚本语言架构思想（字节码操作码、字典哈希化、import 缓存、函数闭包等），但所有代码均为原创实现。

---

## 核心特性

| 类别 | 说明 |
|---|---|
| 数据类型 | 整数 / 浮点 / 字符串（含三引号）/ 布尔 / 空值 / 列表 / 字典 / 函数 / 模块 |
| 流程控制 | `when`/`whenelse`/`otherwise` 分支、`loop` 条件循环、`foreach` 遍历、`stop`/`next` 跳转 |
| 函数 | `proc` 定义、默认参数、递归、闭包 |
| 异常 | `attempt`/`seize`/`settle`/`fling` 异常捕获与抛出 |
| 模块 | `use` 加载模块、模块缓存、命名空间属性访问 |
| 切片 | 支持负索引与缺省值的切片表达式 |
| GUI（Windows） | `window`/`label`/`button`/`textbox`/`msgbox` 等内置函数 |
| 性能优化 | 枚举操作码分发、有序哈希字典、哈希环境、值空闲链表、关键字二分查找、模块缓存 |

---

## 目录结构

```
zunjin/
├── Entry/              # 程序入口（main.c）
├── Syntax/             # 语法层
│   ├── lexer/          #   词法分析器
│   └── parser_*.c      #   语法解析器（core/stmt/expr）
├── Core/               # 核心运行时
│   ├── ast.c           #   AST 节点管理
│   ├── env.c           #   环境与作用域
│   ├── vm_*.c          #   虚拟机（core/stmt/expr/module）
│   └── output.c        #   输出模块
├── Runtime/            # 运行时对象系统（object.c）
├── Builtins/           # 内置函数（builtins.c）
├── Headers/            # 全部头文件
├── Platform/           # 平台相关代码（editor.c 等）
├── Samples/            # 示例脚本（.z）
├── tests/              # 测试脚本（.z）
├── Docs/               # 文档
│   ├── language.md     #   语言参考手册
│   ├── about.zj
│   ├── faq/            #   常见问题
│   ├── library/        #   标准库文档
│   ├── using/          #   使用指南
│   ├── whatsnew/       #   更新日志
│   └── c-api/          #   C API 文档
├── DevNotes/           # 架构笔记
├── BuildScripts/       # 构建脚本（build.bat / clean.bat）
├── Resources/          # 杂项资源（致谢/新闻/历史等）
├── LangSpec/           # 语法规范（grammar.txt）
├── Makefile            # 跨平台编译配置
├── BUILD.md            # 构建指南
├── LICENSE             # 许可证
└── zunjin.exe          # 编译产物
```

---

## 构建

构建方法详见 [BUILD.md](BUILD.md)（支持 Makefile、Windows 批处理、直接编译三种方式）。

---

## 快速上手

### 运行脚本

```bash
zunjin Samples/demo.z
```

### 交互模式（REPL）

```bash
zunjin          # 无参数进入 REPL
```

```
zunjin 交互模式 v0.2.4（输入 exit 或 quit 退出）
支持多行输入：以冒号结尾的语句进入代码块，块内行以空格缩进
----------------------------------------
zunjin> put('你好，zunjin！')
你好，zunjin！
zunjin>
```

### 第一个程序

```zunjin
# hello.z
put('你好，zunjin！')          # 输出

score = 85                     # 变量直接赋值，无需声明类型
when score >= 90:
    put('优秀')
whenelse score >= 60:
    put('及格')
otherwise:
    put('不及格')

i = 1
loop i <= 3:
    put(i)
    i = i + 1
```

---

## 关键字总表

zunjin 共 22 个关键字，**全部自创**，不与任何主流脚本语言关键字重名：

| 关键字 | 用途 | 示例 |
|---|---|---|
| `when` | 条件判断 | `when x > 0:` |
| `whenelse` | 否则如果 | `whenelse x == 0:` |
| `otherwise` | 否则 | `otherwise:` |
| `loop` | while 循环 | `loop i < 10:` |
| `foreach` | for 循环 | `foreach x of lst:` |
| `of` | 遍历/成员测试 | `foreach x of lst`、`3 of [1,2,3]` |
| `proc` | 定义函数 | `proc add(a, b):` |
| `give` | 函数返回值 | `give a + b` |
| `both` | 逻辑与（短路） | `a > 0 both b > 0` |
| `either` | 逻辑或（短路） | `a == 1 either b == 1` |
| `negate` | 逻辑非 / 不在 | `negate ok`、`'x' negate of s` |
| `yes` | 布尔真 | `ok = yes` |
| `no` | 布尔假 | `ok = no` |
| `nil` | 空值 | `x = nil` |
| `stop` | 跳出循环 | `stop` |
| `next` | 继续下一轮循环 | `next` |
| `use` | 导入模块 | `use 'math'` |
| `skip` | 空语句占位 | `skip` |
| `attempt` | 尝试（try） | `attempt:` |
| `seize` | 捕获异常 | `seize err:` |
| `settle` | 最终执行（finally） | `settle:` |
| `fling` | 抛出异常 | `fling '错误信息'` |

> `if / elif / else / while / for / in / def / return / and / or / not / true / false / none / break / continue / import / pass / try / except / finally / raise` 等单词在 zunjin 中只是普通标识符。

---

## 数据类型

| 类型 | 写法 | 示例 |
|---|---|---|
| 整数 | 十进制、进制前缀、下划线 | `42`、`0x1F`(31)、`0o17`(15)、`0b1010`(10)、`1_000_000` |
| 小数 | 小数点、科学计数法 | `3.14`、`1.5e3`、`2.5e-2` |
| 字符串 | 单引号/双引号/三引号 | `'中文'`、`"hello"`、`"""多行"""` |
| 布尔 | `yes` / `no` | `ok = yes` |
| 空值 | `nil` | `x = nil` |
| 列表 | 方括号 | `[1, 2, 3]` |
| 字典 | 花括号 | `{'name': '张三', 'age': 18}` |
| 函数 | `proc` 定义 | `proc f(x): give x * 2` |
| 模块 | `use` 导入 | `use math` |

列表和字典支持**跨行书写**与**尾随逗号**：

```zunjin
students = [
    {'name': '张三', 'scores': [85, 92]},
    {'name': '李四', 'scores': [60, 75]},
]
```

---

## 运算符

| 类别 | 运算符 |
|---|---|
| 算术 | `+ - * / %`、`//` 整除（向下取整）、`**` 幂 |
| 位运算 | `<< >> & \| ^ ~` |
| 比较 | `== != < > <= >=` |
| 成员测试 | `x of list`（在…中）、`x negate of list`（不在…中） |
| 逻辑 | `both`（与）、`either`（或）、`negate`（非），均短路求值 |
| 复合赋值 | `+= -= *= /= //= %= **=` |
| 拼接 | 字符串 `+`、列表 `+` / `*` |
| 切片 | `lst[start:stop:step]`（支持负索引与缺省值） |

```zunjin
put(-7 // 2)              # -4（向下取整）
put(2 ** 10)              # 1024
put(3 of [1, 2, 3])       # yes
put('x' negate of 'abc')  # yes
put(0 both 100)           # 0（短路，返回左值）
put(1 either 100)         # 1
```

---

## 控制流

### 条件分支

```zunjin
when 条件:
    ...
whenelse 条件2:
    ...
otherwise:
    ...
```

分支体用**缩进**表示（4 空格），不需要花括号。

### 循环

```zunjin
# loop：条件循环
i = 0
loop i < 5:
    i = i + 1
    when i == 2:
        next          # 跳过本轮
    when i > 4:
        stop          # 跳出循环
    put(i)

# foreach：遍历列表 / 字典 / 字符串
foreach s of ['a', 'b', 'c']:
    put(s)

foreach k of {'name': 'zunjin', 'age': 1}:
    put(k)            # 遍历字典得到键

foreach ch of 'zjn':
    put(ch)           # 遍历字符串得到字符
```

---

## 函数

```zunjin
proc 函数名(参数1, 参数2 = 默认值):
    ...
    give 返回值      # 返回；不写 give 则返回 nil

proc add(a, b):
    give a + b

proc greet(name, msg = '你好'):
    give msg + ', ' + name

put(add(3, 4))              # 7
put(greet('小明'))           # 你好, 小明
put(greet('小明', '嗨'))     # 嗨, 小明
```

递归、嵌套调用均支持（递归深度默认有限制 512，超出报错不崩溃）。

---

## 异常处理

```zunjin
attempt:
    x = 10 / 0              # 触发异常
seize err:                  # err 是错误信息变量（可省略）
    put('捕获: ' + err)
settle:
    put('无论如何都会执行')

# 主动抛出异常
attempt:
    fling '自定义错误'
seize e:
    put('捕获2: ' + e)
```

异常捕获后程序继续运行，不会崩溃。

---

## 模块系统

`use` 语句加载模块，支持模块缓存与命名空间隔离。

```zunjin
use math          # 加载 StdLib/math/__init__.z，绑定名 math
put(math.pi)      # 属性访问
put(math.abs(-5))
```

- 模块只加载一次，重复 `use` 命中缓存，不会重复执行。
- 模块搜索顺序：当前目录 `name.z` / `name/__init__.z` → 当前目录 `StdLib/...` → 可执行文件目录 → 可执行文件目录 `StdLib/...`。
- 模块是独立命名空间：模块内的变量/函数通过 `模块名.成员` 访问。
- 模块内函数自动获得模块命名空间作为闭包，可访问模块级常量。

### 自定义模块

在脚本同目录放一个 `mymod.z`：

```zunjin
# mymod.z
proc greet(name):
    give '你好，' + name + '！'
```

主脚本中导入：

```zunjin
use 'mymod'
put(mymod.greet('zunjin'))   # 你好，zunjin！
```

---

## 内置函数

zunjin 提供丰富的内置函数，无需导入即可使用：

| 类别 | 函数 |
|---|---|
| 基础 I/O | `put` `read` `input` |
| 序列操作 | `upto` `size` `array` `push` `pop` `insert` `erase` `delete` `clear` `extend` `count` `index` |
| 类型转换 | `text` `integer` `decimal` `boolean` `typeof` `ord` `chr` `hex` `oct` `bin` |
| 数值计算 | `minimum` `maximum` `total` `sum` `absolute` `round` `power` `root` `sqrt` `floor` `ceiling` |
| 排序与反转 | `sort` `reverse` |
| 字符串处理 | `split` `join` `replace` `find` `strip` `lstrip` `rstrip` `upper` `lower` `startswith` `endswith` |
| 迭代器组合 | `enumerate` `zip` |
| 字典操作 | `keys` `values` |

```zunjin
put(upto(5))                 # [0, 1, 2, 3, 4]
put(size([1, 2, 3]))         # 3
put(text(3.14))              # "3.14"
put(typeof(7 / 2))           # float
put(sort([5, 3, 8, 1]))     # [1, 3, 5, 8]
put(split('a,b,c', ','))     # [a, b, c]
put(join('-', ['a', 'b']))   # a-b
put(sqrt(16))                # 4.0
put(total(1, 2, 3))          # 6
```

---

## GUI 窗口编程

**仅 Windows 平台**支持，提供一组 GUI 内置函数：

| 函数 | 作用 |
|---|---|
| `window(标题, 宽, 高)` | 创建窗口 |
| `label(窗口, 文本, x, y)` | 添加文本 |
| `button(窗口, 文本, x, y, w, h)` | 添加按钮 |
| `textbox(窗口, x, y, w, h)` | 添加输入框 |
| `show(窗口)` | 显示窗口 |
| `wait_click(窗口)` | 等待按钮点击，返回控件 id |
| `is_open(窗口)` | 窗口是否打开 |
| `close(窗口)` | 关闭窗口 |
| `get_text(控件)` | 读取文本 |
| `set_text(控件, 文本)` | 设置文本 |
| `msgbox(标题, 内容)` | 消息框 |
| `sleep(毫秒)` | 暂停 |

### 迷你计算器示例

```zunjin
win = window('zunjin 计算器', 340, 240)
when win == 0:
    put('错误: 窗口创建失败')
    stop

label(win, '第一个数:', 20, 22)
ta = textbox(win, 100, 18, 200, 26)
tb = textbox(win, 100, 58, 200, 26)
result = label(win, '结果: 请点击按钮', 20, 100)

btn_add  = button(win, '相加', 30, 150, 80, 34)
btn_quit = button(win, '退出', 130, 195, 90, 30)

show(win)

loop is_open(win):
    cid = wait_click(win)
    when cid == 0:
        stop
    when cid == btn_quit:
        close(win)
    whenelse cid == btn_add:
        a = integer(get_text(ta))
        b = integer(get_text(tb))
        set_text(result, '结果: ' + text(a + b))
```

完整程序见 `tests/gui_demo.z`，运行：`zunjin.exe tests/gui_demo.z`。

---

## 架构设计

zunjin 解释器采用 **AST 树遍历执行**模型：

```
源码 → 词法（lexer）→ 语法（parser）→ AST → VM
```

### 性能优化（独立实现）

| 优化 | 实现位置 |
|---|---|
| 枚举操作码分发（消除 strcmp） | `Headers/ast.h` `ZjnOp`、`Core/vm_*.c` |
| 有序哈希字典（插入序 + O(1) 查找） | `Runtime/object.c` |
| 哈希环境 + 参数所有权转移 | `Core/env.c`、`Core/vm_*.c` |
| 值空闲链表（小对象复用） | `Runtime/object.c` |
| 函数链哈希化（O(1) 内置查找） | `Core/env.c` `func_slots` |
| 关键字二分查找 | `Syntax/lexer/lexer.c` |
| Token 空闲链表 | `Syntax/lexer/lexer.c` |
| 模块缓存 + 搜索路径 + 命名空间 | `Core/vm_module.c` |

### 内存管理

- **引用计数**：`copy` 为 O(1) 共享（+1），`free` 为 -1，归零才释放。
- **空闲链表**：引用计数归零的值结构进入空闲链表复用，减少 `malloc/free`。
- **递归深度限制**：默认 512，防止 C 栈溢出。

### 已知边界

- Windows 上 `long` 为 32 位：整数溢出会静默转 `float`。
- 递归深度上限 512（防 C 栈溢出）。

更多架构细节见 [DevNotes/architecture.zj](DevNotes/architecture.zj)。

---

## 文档

- [语言参考手册](Docs/language.md) - 完整语法、关键字与内置函数
- [常见问题](Docs/faq/index.zj)
- [更新日志](Docs/whatsnew/0.1.zj)
- [使用指南](Docs/using/index.zj)
- [C API 文档](Docs/c-api/index.zj)
- [架构笔记](DevNotes/architecture.zj)
- [IDE 架构笔记](DevNotes/idea-architecture.zj)

---

## 示例

| 示例 | 说明 |
|---|---|
| [Samples/demo.z](Samples/demo.z) | 综合演示：变量、条件、循环、函数 |
| [Samples/bench.z](Samples/bench.z) | 性能基准测试 |
| [Samples/modules.z](Samples/modules.z) | 模块系统演示 |
| [Samples/test_fib.z](Samples/test_fib.z) | 斐波那契数列 |
| [tests/lang_feature_test.z](tests/lang_feature_test.z) | 语言特性测试 |
| [tests/gui_demo.z](tests/gui_demo.z) | GUI 窗口演示 |

---
## 联系
  QQ邮箱：2739593363@qq.com

---
## 双许可证 / Dual-License

本项目采用 **标准双许可证模式**（与 Qt / MySQL / iText 等业界成熟方案一致），
**用户二选一**，不可并用。法律基础：所有贡献版权集中到项目权利人。

> 完整法律文本请阅读仓库内 [LICENSE](LICENSE) 文件。
> For the complete legal text, read the repository's [LICENSE](LICENSE) file.

### 选项一 · GNU LGPL-3.0（开源免费 / Open Source, Free）

| 项目 / Item | 说明 / Description |
|-------------|--------------------|
| 费用 / Fee  | 免费 / Free of charge |
| 开源义务 / Obligations | 修改后如再分发，**须**以 LGPL 形式公开对应源码 / Modified redistributions must keep sources open under LGPL-3.0 |
| 可商用 / Commercial use | ✅ 可以，但须始终满足 LGPL 义务 / Allowed, provided LGPL obligations are always met |
| 适用场景 / Best for | 学习 / 研究 / 开源项目集成 / 愿意接受 LGPL 义务的商业用户 |

### 选项二 · 商业许可证（脱离 LGPL / Proprietary Commercial）

| 项目 / Item | 说明 / Description |
|-------------|--------------------|
| 授权方式 / How to obtain | **联系项目权利人**签订书面协议（通常为付费授权）/ Contact the **Copyright Holder** for a written (typically paid) license |
| LGPL 义务 / LGPL obligations | ❌ **完全脱离** —— 修改后可**闭源分发**、OEM 嵌入、SaaS / 云服务集成、产品打包销售 |
| 源码披露 / Source disclosure | **无义务** / No disclosure obligations |
| 适用场景 / Best for | 希望把本项目集成到**闭源 / 专有商业产品**、不愿公开衍生源码的企业或个人 |

### 联系取得商业许可 / Contact for Commercial Licensing

如需取得选项二（商业许可证）授权，请联系项目权利人，并提供以下信息以便评估与报价：

- 公司 / 个人姓名、联系人、邮箱
- 预计用途（产品名、行业、分发模式、预计年出货量 / 席位）
- 期望授权方式（年度 / 永久、是否需要再许可、是否需要技术支持 SLA）

