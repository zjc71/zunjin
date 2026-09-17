# zunjin 语言参考手册

> zunjin 是一门**完全独立**的编程语言，拥有自创的关键字体系，不兼容任何其他语言关键字。
> 本手册覆盖全部语法、关键字与内置函数用法。

- 文件扩展名：`.z`
- 运行方式：`zunjin.exe 脚本.z`，无参数进入交互模式（REPL）
- 源码编码：UTF-8（支持中文）

---

## 1. 快速上手

```zunjin
# 第一个程序：输出 + 变量 + 条件 + 循环
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

## 2. 关键字总表（22 个，全部自创）

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

> 语言中不存在 `if / elif / else / while / for / in / def / return / and / or / not / true / false / none / break / continue / import / pass / try / except / finally / raise` 等其他语言的关键字——这些单词在 zunjin 中只是普通标识符。

---

## 3. 数据类型与字面量

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

列表和字典支持**跨行书写**与**尾随逗号**：

```zunjin
students = [
    {'name': '张三', 'scores': [85, 92]},
    {'name': '李四', 'scores': [60, 75]},
]
```

---

## 4. 运算符

| 类别 | 运算符 |
|---|---|
| 算术 | `+ - * / %`、`//` 整除（向下取整）、`**` 幂 |
| 位运算 | `<< >> & \| ^ ~` |
| 比较 | `== != < > <= >=` |
| 成员测试 | `x of list`（在…中）、`x negate of list`（不在…中） |
| 逻辑 | `both`（与）、`either`（或）、`negate`（非），均短路求值 |
| 复合赋值 | `+= -= *= /= //= %= **=` |
| 拼接 | 字符串 `+`、列表 `+` / `*` |

```zunjin
put(-7 // 2)              # -4（向下取整）
put(2 ** 10)              # 1024
put(3 of [1, 2, 3])       # yes
put('x' negate of 'abc')  # yes
put(0 both 100)           # 0（短路，返回左值）
put(1 either 100)         # 1
```

---

## 5. 条件分支

```zunjin
when 条件:
    ...
whenelse 条件2:
    ...
whenelse 条件3:
    ...
otherwise:
    ...
```

分支体用**缩进**表示（4 空格），不需要花括号。括号内换行是允许的（zunjin 语义）。

---

## 6. 循环

```zunjin
# loop：条件循环（类似 while）
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

# 结合内置函数生成序列
foreach n of upto(1, 5):      # 1 到 5
    put(n)
```

---

## 7. 函数

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

递归、嵌套调用均支持（递归深度默认有限制，超出报错不崩溃）。

---

## 8. 异常处理

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

## 9. 内置函数速查表

### 基础 I/O（核心是 `put`）

| 函数 | 作用 | 示例 |
|---|---|---|
| `put(...)` | 输出任意内容（可多参数） | `put('结果: ' + 42)` |
| `read()` | 读取一行输入 | `name = read()` |
| `input('提示')` | 带提示读取输入 | `age = input('年龄: ')` |

`put` 的用法：输出字符串、数字、变量、表达式、整个列表/字典均可，多个参数自动空格分隔，字符串与数字可直接用 `+` 拼接。

### 序列操作

`upto`(生成范围) `size`(长度) `array`(建数组) `push`(尾加) `pop`(尾出) `insert`(插入) `erase`(删值) `delete`(按下标删) `clear`(清空) `extend`(合并) `count`(计数) `index`(查找下标)

```zunjin
nums = upto(5)              # [0, 1, 2, 3, 4]
push(nums, 6)               # [0, 1, 2, 3, 4, 6]
x = pop(nums)               # x = 6
put(size(nums))             # 6
```

### 类型转换

`text`(转字符串) `integer`(转整数) `decimal`(转小数) `boolean`(转布尔) `typeof`(查类型) `ord` `chr` `hex` `oct` `bin`

```zunjin
put(text(3.14))             # "3.14"
put(integer('42'))          # 42
put(typeof(7 / 2))          # float
put(hex(255))               # 0xff
```

### 数值计算

`minimum` `maximum` `total` `sum` `absolute` `round`(银行家舍入) `power` `root` `sqrt` `floor` `ceiling`

```zunjin
put(total(1, 2, 3))         # 6
put(total([1, 2, 3]))       # 6（也可传列表）
put(round(2.5))             # 2（银行家舍入）
put(sqrt(16))               # 4.0
```

### 排序与字符串

排序：`sort` `reverse`
字符串：`split` `join` `replace` `find`(找不到返回 -1) `strip` `lstrip` `rstrip` `upper` `lower` `startswith` `endswith`

```zunjin
put(sort([5, 3, 8, 1]))         # [1, 3, 5, 8]
put(split('a,b,c', ','))        # [a, b, c]
put(join('-', ['a', 'b']))      # a-b
put(upper('zjn'))               # ZJN
put(find('hello', 'ell'))       # 1
```

### 迭代器组合与字典

`enumerate` `zip` `keys` `values`

```zunjin
put(enumerate(['a', 'b']))      # [[0, a], [1, b]]
put(zip([1, 2], ['x', 'y']))    # [[1, x], [2, y]]
put(keys({'a': 1, 'b': 2}))     # [a, b]
```

### 窗口程序（Windows GUI）

`sleep`(毫秒) `window`(建窗口) `label`(文本) `button`(按钮) `textbox`(输入框) `show`(显示) `wait_click`(等待点击) `is_open`(窗口是否开着) `close`(关闭) `get_text`(读文字) `set_text`(写文字) `msgbox`(消息框)

---

## 10. 窗口程序示例（迷你计算器）

```zunjin
win = window('zunjin 计算器', 340, 240)     # 建窗口
when win == 0:
    put('错误: 窗口创建失败')
    stop

label(win, '第一个数:', 20, 22)              # 文本
ta = textbox(win, 100, 18, 200, 26)         # 输入框
tb = textbox(win, 100, 58, 200, 26)
result = label(win, '结果: 请点击按钮', 20, 100)

btn_add  = button(win, '相加', 30, 150, 80, 34)
btn_quit = button(win, '退出', 130, 195, 90, 30)

show(win)

loop is_open(win):                           # 事件循环
    cid = wait_click(win)                    # 等待点击，返回控件 id
    when cid == 0:
        stop                                 # 窗口被关闭
    when cid == btn_quit:
        close(win)
    whenelse cid == btn_add:
        a = integer(get_text(ta))            # 读输入并转数字
        b = integer(get_text(tb))
        set_text(result, '结果: ' + text(a + b))
```

完整程序见 `tests\gui_demo.z`，运行：`zunjin.exe tests\gui_demo.z`。

---

## 11. 完整示例：学生成绩统计

```zunjin
students = [
    {'name': '张三', 'scores': [85, 92, 78]},
    {'name': '李四', 'scores': [60, 75, 88]},
]

proc average(lst):
    give total(lst) / size(lst)

proc grade(avg):
    when avg >= 90:
        give 'A'
    whenelse avg >= 60:
        give 'B'
    otherwise:
        give 'C'

foreach s of students:
    avg = average(s['scores'])
    put(s['name'] + ': ' + text(avg) + ' 分，等级 ' + grade(avg))

# 成员测试
names = []
foreach s of students:
    push(names, s['name'])
put('张三' of names)          # yes
```

---

## 12. 模块系统（use 语句）

模块系统提供代码复用与命名空间隔离，设计思路参考主流脚本语言的 import 机制
（模块缓存 + 搜索路径 + 命名空间执行），但为完全独立的实现。

### 12.1 导入模块

```zunjin
use math          # 加载 StdLib/math/__init__.z，绑定名 math
put(math.pi)      # 属性访问
put(math.abs(-5))
```

- 模块只加载一次，重复 `use` 命中缓存（等价 `sys.modules`），不会重复执行。
- 模块搜索顺序：当前目录 `name.z` / `name/__init__.z` →
  当前目录 `StdLib/...` → 可执行文件目录 → 可执行文件目录 `StdLib/...`。
- 模块是独立命名空间：模块内的变量/函数通过 `模块名.成员` 访问；
  未在模块内定义的成员会回退到全局内置函数（如 `math.floor` 直接可用）。
- 模块内函数自动获得模块命名空间作为闭包，可访问模块级常量与互相调用。

### 12.2 自定义模块

在脚本同目录放一个 `mymod.z`：

```zunjin
# mymod.z
proc greet(name):
    give '你好，' + name + '！'
```

```zunjin
# main.z
use mymod
put(mymod.greet('zunjin'))   # 你好，zunjin！
```

包形式：目录 `mymod/__init__.z` 同样会被搜索。

### 12.3 标准库一览（StdLib/）

| 模块 | 内容 |
|---|---|
| `math` | pi/e、abs/min/max/sqrt/pow/ceil/floor/round、factorial/gcd |
| `string` | 字符集常量、capitalize/title/swapcase/count_sub/trim/words |
| `types` | type_of、is_number/is_string/is_list/is_dict/is_bool/is_func |
| `collections` | head/tail/unique/flatten_once/sorted_keys/sum/count_of |
| `functools` | map/filter/reduce（支持函数作为参数传递） |
| `itertools` | repeat/chain/take/drop/count/pairwise |
| `random` | 纯 zunjin LCG：seed/random/randint/choice |
| `json` | dumps/loads（单层数组与对象） |
| `re` | 通配符 match/search（`*` 与 `?`） |
| `logging` | DEBUG/INFO/WARN/ERROR、set_level、debug/info/warn/error |
| `pathlib` | join/basename/dirname/extname/normalize |
| `datetime` | is_leap_year/days_in_month |
| `html` | escape/unescape |
| `urllib` | url_parse/url_encode |
| `enum` | create_enum |
| `contextlib` | suppress |
| `test` | assert_equal/assert_true/assert_false/assert_raises |
| `sys`/`os`/`io`/`http`/`encodings`/`copy` | 常量与基础工具 |

### 12.4 属性访问

`对象.成员` 语法（`.` 后必须跟标识符），当前仅支持模块对象：

```zunjin
use math
x = math.pow(2, 8)     # 256.0
```

---

## 13. 常用约定

- 注释：`#` 开头到行尾
- 缩进：块语句用缩进（建议 4 空格），括号内可自由换行
- 字符串支持转义 `\n` `\t` `\\` `\'` `\"`
- 下标从 0 开始，支持负数（-1 表示最后一个）
- 切片 `s[开始:结束:步长]`，如 `s[::-1]` 逆序
- 写错不崩溃：语法错误/运行时错误会报告"第几行第几列"
