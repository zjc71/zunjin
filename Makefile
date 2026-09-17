# ==========================================================================
# zunjin 编程语言 - Makefile 编译配置
# 支持 Linux/macOS (gcc/clang) 和 Windows (MinGW/msys2)
# ==========================================================================

# ---------- 项目名称 ----------
TARGET      = zunjin
PROJECT     = zunjin

# ---------- 编译器与标志 ----------
CC          = gcc
CFLAGS      = -Wall -Wextra -std=gnu11 -O2 -g
LDFLAGS     = -lm

# 调试模式
ifdef DEBUG
    CFLAGS  = -Wall -Wextra -std=c11 -g -O0 -DDEBUG
endif

# ---------- 目录 ----------
SRC_DIR     = .
INC_DIR     = $(SRC_DIR)/Headers
OBJ_DIR     = obj

# ---------- 源文件 ----------
SOURCES = \
    $(SRC_DIR)/Entry/main.c \
    $(SRC_DIR)/Syntax/lexer/lexer.c \
    $(SRC_DIR)/Syntax/parser_core.c \
    $(SRC_DIR)/Syntax/parser_stmt.c \
    $(SRC_DIR)/Syntax/parser_expr.c \
    $(SRC_DIR)/Core/ast.c \
    $(SRC_DIR)/Core/vm_core.c \
    $(SRC_DIR)/Core/vm_stmt.c \
    $(SRC_DIR)/Core/vm_expr.c \
    $(SRC_DIR)/Core/vm_module.c \
    $(SRC_DIR)/Core/env.c \
    $(SRC_DIR)/Core/output.c \
    $(SRC_DIR)/Runtime/object.c \
    $(SRC_DIR)/Builtins/builtins.c

# ---------- 对象文件 ----------
OBJECTS = $(SOURCES:$(SRC_DIR)/%.c=$(OBJ_DIR)/%.o)

# ---------- 头文件依赖 ----------
DEPS = $(wildcard $(INC_DIR)/*.h)

# ---------- 平台检测 ----------
ifneq ($(OS),Windows_NT)
    # Linux/macOS
    RM          = rm -rf
    TARGET_EXE  = $(TARGET)
    RUN_PREFIX  = ./
    FIX_PATH    =
else
    # Windows
    RM          = del /Q /S
    TARGET_EXE  = $(TARGET).exe
    RUN_PREFIX  =
    OBJ_DIR     = obj_win
    CFLAGS      += -D_WIN32
    # Windows 下 mkdir 兼容
    MKDIR       = if not exist
    MKDIR_CMD   = $(subst /,\,$(dir $@))
endif

# ---------- 编译规则 ----------
.PHONY: all clean run test demo debug rebuild

all: $(TARGET_EXE)

# 链接
$(TARGET_EXE): $(OBJECTS)
	$(CC) $(CFLAGS) -o $@ $^ $(LDFLAGS)
	@echo "========================================"
	@echo "  zunjin 编程语言编译完成"
	@echo "  用法: $(RUN_PREFIX)$(TARGET_EXE) Examples/demo.z"
	@echo "========================================"

# 编译
$(OBJ_DIR)/%.o: $(SRC_DIR)/%.c $(DEPS)
ifneq ($(OS),Windows_NT)
	@mkdir -p $(dir $@)
else
	@if not exist "$(subst /,\,$(dir $@))" mkdir "$(subst /,\,$(dir $@))"
endif
	$(CC) $(CFLAGS) -I$(INC_DIR) -c $< -o $@

# ---------- 运行 ----------
run: $(TARGET_EXE)
	$(RUN_PREFIX)$(TARGET_EXE) Samples/demo.z

demo: $(TARGET_EXE)
	$(RUN_PREFIX)$(TARGET_EXE) Samples/demo.z

# ---------- 测试 ----------
test: $(TARGET_EXE)
	@echo "运行测试..."
	$(RUN_PREFIX)$(TARGET_EXE) Samples/demo.z

# ---------- 调试 ----------
debug:
	$(MAKE) DEBUG=1

# ---------- 清理 ----------
clean:
ifneq ($(OS),Windows_NT)
	rm -rf $(OBJ_DIR)
	rm -f $(TARGET_EXE)
else
	if exist $(OBJ_DIR) rmdir /S /Q $(OBJ_DIR)
	if exist $(TARGET_EXE) del /Q $(TARGET_EXE)
endif
	@echo "清理完成"

# ---------- 重新编译 ----------
rebuild: clean all