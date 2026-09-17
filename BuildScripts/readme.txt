zunjin 构建配置
================

本目录包含 zunjin 的跨平台构建脚本。

构建方法:
1. Windows: 双击运行 build.bat（或命令行执行）
2. 跨平台: 在项目根目录执行 make
3. 调试模式: make DEBUG=1

依赖:
- MinGW-w64 或 MSYS2 (gcc)
- 或 Visual Studio (MSVC)
- 或任意支持 C11 的 C 编译器

清理:
- Windows: 运行 clean.bat
- 跨平台: make clean
