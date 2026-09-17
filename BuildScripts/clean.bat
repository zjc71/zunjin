@echo off
REM zunjin 清理脚本

setlocal
cd /d "%~dp0.."

REM 尝试 make clean
set DONE=0
where mingw32-make >nul 2>nul
if %ERRORLEVEL% EQU 0 (
    mingw32-make clean
    set DONE=1
)
if %DONE% EQU 0 (
    where make >nul 2>nul
    if %ERRORLEVEL% EQU 0 (
        make clean
        set DONE=1
    )
)

REM 如果没有 make，手动清理
if %DONE% EQU 0 (
    if exist obj_win rmdir /S /Q obj_win
    if exist zunjin.exe del /Q zunjin.exe
)
echo 清理完成
endlocal