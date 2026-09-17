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
 * zunjin 编程语言 - 代码编辑器 (Win32 GUI)
 * 编译: gcc -mwindows -o zunjin-edit.exe editor.c -lgdi32 -lcomctl32 -lcomdlg32
 * 用法: zunjin-edit.exe [文件.z]
 * ========================================================================== */

#define WIN32_LEAN_AND_MEAN
#define UNICODE
#define _UNICODE
#include <windows.h>
#include <commctrl.h>
#include <commdlg.h>
#include <richedit.h>
#include <stdio.h>
#include <string.h>
#include <stdlib.h>

#pragma comment(lib, "comctl32.lib")
#pragma comment(lib, "comdlg32.lib")
#pragma comment(lib, "msftedit.lib")

/* ---------- 控件 ID ---------- */
#define ID_FILE_NEW     1001
#define ID_FILE_OPEN    1002
#define ID_FILE_SAVE    1003
#define ID_FILE_SAVEAS  1004
#define ID_FILE_EXIT    1005
#define ID_RUN_RUN      2001
#define ID_EDITOR_MAIN  3001
#define ID_OUTPUT_MAIN  3002
#define ID_TOOLBAR      3003
#define ID_STATUS_BAR   3004
#define ID_TAB_EDITOR   4001
#define ID_TAB_OUTPUT   4002

/* ---------- 全局 ---------- */
HINSTANCE g_hInst;
HWND g_hEdit, g_hOutput, g_hStatus, g_hTab, g_hWnd;
wchar_t g_currentFile[1024] = {0};
int g_fileModified = 0;

/* ---------- 前向声明 ---------- */
LRESULT CALLBACK WndProc(HWND, UINT, WPARAM, LPARAM);
void FileOpen(HWND);
void FileSave(HWND);
void FileSaveAs(HWND);
void RunScript(HWND);
void UpdateTitle(HWND);
void SetStatusText(const wchar_t*);

/* ===================================================================
 *  程序入口
 * =================================================================== */
int WINAPI WinMain(HINSTANCE hInstance, HINSTANCE hPrev,
                   LPSTR lpCmdLine, int nCmdShow) {
    (void)hPrev;
    g_hInst = hInstance;

    /* 初始化通用控件 */
    INITCOMMONCONTROLSEX icex = { sizeof(INITCOMMONCONTROLSEX),
                                 ICC_COOL_CLASSES | ICC_BAR_CLASSES |
                                 ICC_TAB_CLASSES };
    InitCommonControlsEx(&icex);

    /* 加载 Rich Edit 库 */
    LoadLibraryW(L"Msftedit.dll");

    /* 注册窗口 */
    const wchar_t CLASS[] = L"ZunjinEditor";
    WNDCLASSW wc = {0};
    wc.lpfnWndProc   = WndProc;
    wc.hInstance     = hInstance;
    wc.hCursor       = LoadCursor(NULL, IDC_ARROW);
    wc.hbrBackground = (HBRUSH)(COLOR_WINDOW + 1);
    wc.lpszClassName = CLASS;
    wc.lpszMenuName  = NULL;
    wc.hIcon         = LoadIcon(NULL, IDI_APPLICATION);
    RegisterClassW(&wc);

    /* 窗口位置 */
    int sw = GetSystemMetrics(SM_CXSCREEN);
    int sh = GetSystemMetrics(SM_CYSCREEN);
    int ww = 900, wh = 650;
    int wx = (sw - ww) / 2, wy = (sh - wh) / 2;

    g_hWnd = CreateWindowExW(0, CLASS, L"zunjin 代码编辑器",
                WS_OVERLAPPEDWINDOW, wx, wy, ww, wh,
                NULL, NULL, hInstance, NULL);
    if (!g_hWnd) return 1;

    ShowWindow(g_hWnd, nCmdShow);
    UpdateWindow(g_hWnd);

    /* 如果命令行有文件参数，打开它 */
    if (lpCmdLine && lpCmdLine[0]) {
        int len = MultiByteToWideChar(CP_UTF8, 0, lpCmdLine, -1, NULL, 0);
        if (len > 0) {
            wchar_t* wpath = (wchar_t*)malloc(len * sizeof(wchar_t));
            MultiByteToWideChar(CP_UTF8, 0, lpCmdLine, -1, wpath, len);
            wcscpy(g_currentFile, wpath);
            free(wpath);
            FileOpen(g_hWnd);
        }
    }

    /* 消息循环 */
    MSG msg;
    while (GetMessage(&msg, NULL, 0, 0)) {
        TranslateMessage(&msg);
        DispatchMessage(&msg);
    }
    return 0;
}

/* ===================================================================
 *  窗口过程
 * =================================================================== */
LRESULT CALLBACK WndProc(HWND hwnd, UINT msg, WPARAM w, LPARAM l) {
    switch (msg) {
        case WM_CREATE: {
            /* 创建菜单 */
            HMENU hMenu = CreateMenu();
            HMENU hFile = CreatePopupMenu();
            AppendMenuW(hFile, MF_STRING, ID_FILE_NEW,    L"新建(&N)");
            AppendMenuW(hFile, MF_STRING, ID_FILE_OPEN,   L"打开(&O)");
            AppendMenuW(hFile, MF_SEPARATOR, 0, NULL);
            AppendMenuW(hFile, MF_STRING, ID_FILE_SAVE,   L"保存(&S)");
            AppendMenuW(hFile, MF_STRING, ID_FILE_SAVEAS, L"另存为(&A)");
            AppendMenuW(hFile, MF_SEPARATOR, 0, NULL);
            AppendMenuW(hFile, MF_STRING, ID_FILE_EXIT,   L"退出(&X)");
            AppendMenuW(hMenu, MF_POPUP, (UINT_PTR)hFile, L"文件(&F)");

            HMENU hRun = CreatePopupMenu();
            AppendMenuW(hRun, MF_STRING, ID_RUN_RUN,  L"运行(&R)");
            AppendMenuW(hMenu, MF_POPUP, (UINT_PTR)hRun, L"运行(&R)");

            SetMenu(hwnd, hMenu);

            /* 创建工具栏 */
            HWND hTool = CreateWindowExW(0, TOOLBARCLASSNAMEW, NULL,
                WS_CHILD | WS_VISIBLE | TBSTYLE_FLAT | TBSTYLE_TOOLTIPS |
                CCS_ADJUSTABLE | CCS_NODIVIDER,
                0, 0, 0, 0, hwnd, (HMENU)ID_TOOLBAR, g_hInst, NULL);

            SendMessageW(hTool, TB_BUTTONSTRUCTSIZE, sizeof(TBBUTTON), 0);

            TBBUTTON tbBtns[] = {
                {0, ID_FILE_NEW,  TBSTATE_ENABLED, BTNS_BUTTON, {0}, 0, (INT_PTR)L"新建"},
                {1, ID_FILE_OPEN, TBSTATE_ENABLED, BTNS_BUTTON, {0}, 0, (INT_PTR)L"打开"},
                {2, ID_FILE_SAVE, TBSTATE_ENABLED, BTNS_BUTTON, {0}, 0, (INT_PTR)L"保存"},
                {0, 0, TBSTATE_ENABLED, BTNS_SEP, {0}, 0, 0},
                {3, ID_RUN_RUN,   TBSTATE_ENABLED, BTNS_BUTTON, {0}, 0, (INT_PTR)L"运行"},
            };
            SendMessageW(hTool, TB_ADDBUTTONS, 5, (LPARAM)tbBtns);

            /* 获取客户区尺寸 */
            RECT rc;
            GetClientRect(hwnd, &rc);
            int toolH = 28;
            int tabH = 22;
            int statusH = 22;
            int outputH = 140;

            int tabY = toolH;
            int editY = tabY + tabH;
            int editH = (rc.bottom - statusH - outputH) - editY;
            int outputY = editY + editH;

            /* 文件标签 */
            g_hTab = CreateWindowW(WC_TABCONTROLW, NULL,
                WS_CHILD | WS_VISIBLE | TCS_FIXEDWIDTH,
                0, tabY, rc.right, tabH,
                hwnd, (HMENU)ID_TAB_EDITOR, g_hInst, NULL);

            TCITEMW tie = {0};
            tie.mask = TCIF_TEXT;
            wchar_t tabText[] = L" 未命名.z ";
            tie.pszText = tabText;
            TabCtrl_InsertItem(g_hTab, 0, &tie);

            wchar_t tabOut[] = L" 输出 ";
            tie.pszText = tabOut;
            TabCtrl_InsertItem(g_hTab, 1, &tie);

            /* 主编辑器 (Rich Edit 4.1 - msftedit) */
            g_hEdit = CreateWindowExW(WS_EX_CLIENTEDGE,
                L"RichEdit50W", NULL,
                WS_CHILD | WS_VISIBLE | ES_MULTILINE | ES_WANTRETURN |
                ES_AUTOHSCROLL | ES_AUTOVSCROLL | WS_HSCROLL | WS_VSCROLL,
                0, editY, rc.right, editH,
                hwnd, (HMENU)ID_EDITOR_MAIN, g_hInst, NULL);

            /* 设置编辑器字体 */
            CHARFORMAT2W cf = {0};
            cf.cbSize = sizeof(cf);
            cf.dwMask = CFM_FACE | CFM_SIZE | CFM_CHARSET;
            cf.bCharSet = DEFAULT_CHARSET;
            cf.yHeight = 200;
            wcscpy(cf.szFaceName, L"Consolas");
            SendMessageW(g_hEdit, EM_SETCHARFORMAT, SCF_ALL, (LPARAM)&cf);

            /* 设置 Tab 宽度为 4 空格 */
            int tabStops = 16;
            SendMessageW(g_hEdit, EM_SETTABSTOPS, 1, (LPARAM)&tabStops);

            /* 输出面板 */
            g_hOutput = CreateWindowExW(WS_EX_CLIENTEDGE,
                L"EDIT", NULL,
                WS_CHILD | WS_VISIBLE | ES_MULTILINE | ES_READONLY |
                ES_AUTOVSCROLL | WS_VSCROLL,
                0, outputY, rc.right, outputH,
                hwnd, (HMENU)ID_OUTPUT_MAIN, g_hInst, NULL);

            /* 状态栏 */
            g_hStatus = CreateWindowW(STATUSCLASSNAMEW, NULL,
                WS_CHILD | WS_VISIBLE | SBARS_SIZEGRIP,
                0, 0, 0, 0, hwnd, (HMENU)ID_STATUS_BAR, g_hInst, NULL);

            int parts[] = {300, 500, -1};
            SendMessageW(g_hStatus, SB_SETPARTS, 3, (LPARAM)parts);
            SetStatusText(L"就绪");

            SetFocus(g_hEdit);
            break;
        }

        case WM_SIZE: {
            RECT rc;
            GetClientRect(hwnd, &rc);
            int toolH = 28;
            int tabH = 22;
            int statusH = 22;
            int outputH = 140;

            int tabY = toolH;
            int editY = tabY + tabH;
            int editH = (rc.bottom - statusH - outputH) - editY;
            int outputY = editY + editH;

            HWND hTool = GetDlgItem(hwnd, ID_TOOLBAR);
            if (hTool) {
                SendMessageW(hTool, TB_AUTOSIZE, 0, 0);
                SetWindowPos(hTool, NULL, 0, 0, rc.right, toolH, SWP_NOZORDER);
            }
            if (g_hTab)
                SetWindowPos(g_hTab, NULL, 0, tabY, rc.right, tabH, SWP_NOZORDER);
            if (g_hEdit)
                SetWindowPos(g_hEdit, NULL, 0, editY, rc.right, editH, SWP_NOZORDER);
            if (g_hOutput)
                SetWindowPos(g_hOutput, NULL, 0, outputY, rc.right, outputH, SWP_NOZORDER);
            if (g_hStatus)
                SendMessageW(g_hStatus, WM_SIZE, 0, 0);
            break;
        }

        case WM_NOTIFY: {
            if (((LPNMHDR)l)->idFrom == ID_TAB_EDITOR &&
                ((LPNMHDR)l)->code == TCN_SELCHANGE) {
                int sel = TabCtrl_GetCurSel(g_hTab);
                ShowWindow(g_hEdit, sel == 0 ? SW_SHOW : SW_HIDE);
                ShowWindow(g_hOutput, sel == 1 ? SW_SHOW : SW_HIDE);
            }
            break;
        }

        case WM_COMMAND: {
            switch (LOWORD(w)) {
                case ID_FILE_NEW: {
                    if (g_fileModified) {
                        int ret = MessageBoxW(hwnd,
                            L"文件已修改，是否保存？", L"zunjin 编辑器",
                            MB_YESNOCANCEL | MB_ICONQUESTION);
                        if (ret == IDYES) FileSave(hwnd);
                        else if (ret == IDCANCEL) break;
                    }
                    SetWindowTextW(g_hEdit, L"");
                    g_currentFile[0] = L'\0';
                    g_fileModified = 0;
                    UpdateTitle(hwnd);
                    SetStatusText(L"新建文件");
                    break;
                }

                case ID_FILE_OPEN:
                    FileOpen(hwnd);
                    break;

                case ID_FILE_SAVE:
                    FileSave(hwnd);
                    break;

                case ID_FILE_SAVEAS:
                    FileSaveAs(hwnd);
                    break;

                case ID_FILE_EXIT:
                    SendMessageW(hwnd, WM_CLOSE, 0, 0);
                    break;

                case ID_RUN_RUN:
                    RunScript(hwnd);
                    break;

                default:
                    return DefWindowProcW(hwnd, msg, w, l);
            }
            break;
        }

        case WM_CLOSE: {
            if (g_fileModified) {
                int ret = MessageBoxW(hwnd,
                    L"文件已修改，是否保存？", L"zunjin 编辑器",
                    MB_YESNOCANCEL | MB_ICONQUESTION);
                if (ret == IDYES) FileSave(hwnd);
                else if (ret == IDCANCEL) return 0;
            }
            DestroyWindow(hwnd);
            break;
        }

        case WM_DESTROY:
            PostQuitMessage(0);
            break;

        default:
            return DefWindowProcW(hwnd, msg, w, l);
    }
    return 0;
}

/* ===================================================================
 *  文件操作
 * =================================================================== */

/* 打开文件 */
void FileOpen(HWND hwnd) {
    /* 检查是否保存 */
    if (g_fileModified) {
        int ret = MessageBoxW(hwnd,
            L"文件已修改，是否保存？", L"zunjin 编辑器",
            MB_YESNOCANCEL | MB_ICONQUESTION);
        if (ret == IDYES) FileSave(hwnd);
        else if (ret == IDCANCEL) return;
    }

    /* 如果已有路径，直接打开 */
    if (g_currentFile[0] == L'\0') {
        OPENFILENAMEW ofn = {0};
        ofn.lStructSize = sizeof(ofn);
        ofn.hwndOwner = hwnd;
        wchar_t path[1024] = {0};
        ofn.lpstrFile = path;
        ofn.nMaxFile = 1024;
        ofn.lpstrFilter = L"zunjin 脚本 (*.z)\0*.z\0所有文件 (*.*)\0*.*\0";
        ofn.lpstrDefExt = L"z";
        ofn.Flags = OFN_FILEMUSTEXIST | OFN_HIDEREADONLY;

        if (!GetOpenFileNameW(&ofn)) return;
        wcscpy(g_currentFile, path);
    }

    /* 读取文件 (UTF-8 编码) */
    FILE* fp = _wfopen(g_currentFile, L"rb");
    if (!fp) {
        SetStatusText(L"打开文件失败");
        return;
    }

    fseek(fp, 0, SEEK_END);
    long size = ftell(fp);
    fseek(fp, 0, SEEK_SET);

    char* buf = (char*)malloc(size + 1);
    if (!buf) { fclose(fp); return; }
    fread(buf, 1, size, fp);
    buf[size] = '\0';
    fclose(fp);

    /* 检测 BOM 并跳过 */
    int offset = 0;
    if (size >= 3 && (unsigned char)buf[0] == 0xEF &&
                     (unsigned char)buf[1] == 0xBB &&
                     (unsigned char)buf[2] == 0xBF) {
        offset = 3;
    }

    /* 转换为宽字符 */
    int wlen = MultiByteToWideChar(CP_UTF8, 0, buf + offset, -1, NULL, 0);
    wchar_t* wbuf = (wchar_t*)malloc(wlen * sizeof(wchar_t));
    MultiByteToWideChar(CP_UTF8, 0, buf + offset, -1, wbuf, wlen);
    free(buf);

    SetWindowTextW(g_hEdit, wbuf);
    free(wbuf);

    g_fileModified = 0;
    UpdateTitle(hwnd);

    wchar_t status[512];
    swprintf(status, 512, L"已打开: %s", g_currentFile);
    SetStatusText(status);

    /* 更新标签名 */
    const wchar_t* name = wcsrchr(g_currentFile, L'\\');
    name = name ? name + 1 : g_currentFile;
    TCITEMW tie = {0};
    tie.mask = TCIF_TEXT;
    wchar_t tabText[256];
    swprintf(tabText, 256, L" %s ", name);
    tie.pszText = tabText;
    TabCtrl_SetItem(g_hTab, 0, &tie);
}

/* 保存文件 */
void FileSave(HWND hwnd) {
    if (g_currentFile[0] == L'\0') {
        FileSaveAs(hwnd);
        return;
    }

    /* 获取编辑器文本 */
    int len = GetWindowTextLengthW(g_hEdit);
    wchar_t* wbuf = (wchar_t*)malloc((len + 1) * sizeof(wchar_t));
    GetWindowTextW(g_hEdit, wbuf, len + 1);

    /* 转换为 UTF-8 */
    int utf8len = WideCharToMultiByte(CP_UTF8, 0, wbuf, -1, NULL, 0, NULL, NULL);
    char* utf8buf = (char*)malloc(utf8len);
    WideCharToMultiByte(CP_UTF8, 0, wbuf, -1, utf8buf, utf8len, NULL, NULL);
    free(wbuf);

    /* 写入文件 */
    FILE* fp = _wfopen(g_currentFile, L"wb");
    if (!fp) {
        SetStatusText(L"保存失败！");
        free(utf8buf);
        return;
    }
    fwrite(utf8buf, 1, utf8len - 1, fp);
    fclose(fp);
    free(utf8buf);

    g_fileModified = 0;
    UpdateTitle(hwnd);

    wchar_t status[512];
    swprintf(status, 512, L"已保存: %s", g_currentFile);
    SetStatusText(status);
}

/* 另存为 */
void FileSaveAs(HWND hwnd) {
    OPENFILENAMEW ofn = {0};
    ofn.lStructSize = sizeof(ofn);
    ofn.hwndOwner = hwnd;
    wchar_t path[1024] = {0};
    if (g_currentFile[0])
        wcscpy(path, g_currentFile);
    ofn.lpstrFile = path;
    ofn.nMaxFile = 1024;
    ofn.lpstrFilter = L"zunjin 脚本 (*.z)\0*.z\0所有文件 (*.*)\0*.*\0";
    ofn.lpstrDefExt = L"z";
    ofn.Flags = OFN_OVERWRITEPROMPT | OFN_HIDEREADONLY;

    if (!GetSaveFileNameW(&ofn)) return;
    wcscpy(g_currentFile, path);
    FileSave(hwnd);

    /* 更新标签名 */
    const wchar_t* name = wcsrchr(g_currentFile, L'\\');
    name = name ? name + 1 : g_currentFile;
    TCITEMW tie = {0};
    tie.mask = TCIF_TEXT;
    wchar_t tabText[256];
    swprintf(tabText, 256, L" %s ", name);
    tie.pszText = tabText;
    TabCtrl_SetItem(g_hTab, 0, &tie);
}

/* ===================================================================
 *  运行脚本
 * =================================================================== */
void RunScript(HWND hwnd) {
    /* 先保存 */
    if (g_currentFile[0] == L'\0' || g_fileModified) {
        int ret = MessageBoxW(hwnd, L"运行前需要先保存文件。\n是否保存？",
            L"zunjin 编辑器", MB_YESNO | MB_ICONQUESTION);
        if (ret == IDNO) return;
        FileSave(hwnd);
        if (g_fileModified) return;
    }

    SetStatusText(L"正在运行...");

    /* 清空输出 */
    SetWindowTextW(g_hOutput, L"");

    /* 构建命令: zunjin.exe "文件名" */
    wchar_t cmdLine[2048];
    wchar_t exeDir[1024];
    GetModuleFileNameW(NULL, exeDir, 1024);
    wchar_t* last = wcsrchr(exeDir, L'\\');
    if (last) {
        wcscpy(last + 1, L"zunjin.exe");
        swprintf(cmdLine, 2048, L"\"%s\" \"%s\"", exeDir, g_currentFile);
    } else {
        swprintf(cmdLine, 2048, L"zunjin.exe \"%s\"", g_currentFile);
    }

    /* 创建进程并捕获输出 */
    SECURITY_ATTRIBUTES sa = { sizeof(SECURITY_ATTRIBUTES), NULL, TRUE };
    HANDLE hRead, hWrite;
    CreatePipe(&hRead, &hWrite, &sa, 0);

    PROCESS_INFORMATION pi = {0};
    STARTUPINFOW si = {0};
    si.cb = sizeof(si);
    si.dwFlags = STARTF_USESTDHANDLES;
    si.hStdOutput = hWrite;
    si.hStdError = hWrite;

    BOOL ok = CreateProcessW(NULL, cmdLine, NULL, NULL, TRUE,
                             CREATE_NO_WINDOW, NULL, NULL, &si, &pi);

    CloseHandle(hWrite);

    if (!ok) {
        wchar_t errMsg[512];
        swprintf(errMsg, 512, L"运行失败！\n请确认 zunjin.exe 与编辑器在同一目录下。");
        MessageBoxW(hwnd, errMsg, L"错误", MB_OK | MB_ICONERROR);
        SetStatusText(L"运行失败");
        CloseHandle(hRead);
        return;
    }

    /* 读取输出 (UTF-8 -> UTF-16) */
    char buf[4096];
    DWORD read;
    wchar_t wbuf[4096];
    while (ReadFile(hRead, buf, sizeof(buf) - 1, &read, NULL) && read > 0) {
        buf[read] = '\0';
        int wlen = MultiByteToWideChar(CP_UTF8, 0, buf, read, NULL, 0);
        if (wlen > 0) {
            MultiByteToWideChar(CP_UTF8, 0, buf, read, wbuf, wlen);
            wbuf[wlen] = L'\0';
            int curLen = GetWindowTextLengthW(g_hOutput);
            SendMessageW(g_hOutput, EM_SETSEL, curLen, curLen);
            SendMessageW(g_hOutput, EM_REPLACESEL, FALSE, (LPARAM)wbuf);
        }
    }

    WaitForSingleObject(pi.hProcess, 5000);
    CloseHandle(pi.hProcess);
    CloseHandle(pi.hThread);
    CloseHandle(hRead);

    /* 切换到输出标签 */
    TabCtrl_SetCurSel(g_hTab, 1);
    ShowWindow(g_hEdit, SW_HIDE);
    ShowWindow(g_hOutput, SW_SHOW);

    SetStatusText(L"运行完成");
}

/* ===================================================================
 *  辅助函数
 * =================================================================== */

/* 更新窗口标题 */
void UpdateTitle(HWND hwnd) {
    wchar_t title[1024];
    if (g_currentFile[0]) {
        const wchar_t* name = wcsrchr(g_currentFile, L'\\');
        name = name ? name + 1 : g_currentFile;
        swprintf(title, 1024, L"zunjin 代码编辑器 - %s%s",
                 name, g_fileModified ? L" *" : L"");
    } else {
        swprintf(title, 1024, L"zunjin 代码编辑器 - 未命名%s",
                 g_fileModified ? L" *" : L"");
    }
    SetWindowTextW(hwnd, title);
}

/* 设置状态栏文本 */
void SetStatusText(const wchar_t* text) {
    SendMessageW(g_hStatus, SB_SETTEXT, 0, (LPARAM)text);
}