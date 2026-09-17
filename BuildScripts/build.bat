@echo off
REM zunjin Windows build script

setlocal
chcp 65001 >nul
cd /d "%~dp0.."

REM Try make
set MADE=0
where mingw32-make >nul 2>nul
if %ERRORLEVEL% EQU 0 (
    mingw32-make all
    if %ERRORLEVEL% EQU 0 set MADE=1
)
if %MADE% EQU 0 (
    where make >nul 2>nul
    if %ERRORLEVEL% EQU 0 (
        make all
        if %ERRORLEVEL% EQU 0 set MADE=1
    )
)

REM Exit if make succeeded
if %MADE% EQU 1 (
    echo Build succeeded!
    exit /b 0
)

REM Fallback to direct gcc call
gcc -Wall -Wextra -std=c11 -O2 -IHeaders Entry\main.c Syntax\lexer\lexer.c Syntax\parser_core.c Syntax\parser_stmt.c Syntax\parser_expr.c Core\ast.c Core\vm_core.c Core\vm_stmt.c Core\vm_expr.c Core\vm_module.c Core\env.c Core\output.c Runtime\object.c Builtins\builtins.c -o zunjin.exe -lm
if %ERRORLEVEL% NEQ 0 (
    echo Build failed!
    exit /b 1
)
echo Build succeeded!
exit /b 0