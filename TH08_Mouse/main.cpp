#include <windows.h>
#include <tlhelp32.h>
#include <tchar.h>
#include <cstdio>
using namespace std;

typedef UINT64 QWORD;

static DWORD GetProcessID(const wchar_t* processName) {
    DWORD processID = 0;
    HANDLE snapshot = CreateToolhelp32Snapshot(TH32CS_SNAPPROCESS, 0);
    if (snapshot == INVALID_HANDLE_VALUE) {
        return 0;
    }

    PROCESSENTRY32W pe;
    pe.dwSize = sizeof(PROCESSENTRY32W);
    if (Process32FirstW(snapshot, &pe)) {
        do {
            if (_wcsicmp(pe.szExeFile, processName) == 0) {
                processID = pe.th32ProcessID;
                break;
            }
        } while (Process32NextW(snapshot, &pe));
    }
    CloseHandle(snapshot);
    return processID;
}

const wchar_t* processName = L"th08.exe";
DWORD pid;
HANDLE hd;  //进程句柄

//获取窗口位置的属性
RECT gameWindow; //left, top, right, bottom
POINT mousePos; //x, y
HWND gameHandle;
struct handle_data {
    unsigned long process_id;
    HWND best_handle;
};

static BOOL IsMainWindow(HWND handle)
{
    return GetWindow(handle, GW_OWNER) == (HWND)0 && IsWindowVisible(handle);
}

static BOOL CALLBACK EnumWindowsCallback(HWND handle, LPARAM lParam)
{
    handle_data& data = *(handle_data*)lParam;
    unsigned long process_id = 0;
    GetWindowThreadProcessId(handle, &process_id);
    if (data.process_id != process_id || !IsMainWindow(handle)) {
        return TRUE;
    }
    data.best_handle = handle;
    return FALSE;
}

static HWND FindMainWindow(unsigned long process_id)
{
    handle_data data;
    data.process_id = process_id;
    data.best_handle = 0;
    EnumWindows(EnumWindowsCallback, (LPARAM)&data);
    return data.best_handle;
}

//映射坐标
//游戏窗口内坐标：x轴从左到右8 ~ 376
//              y轴从上到下16 ~ 432  (th08)
//对应窗口的left+lb*width ~ right-rb*width
//          top+tb*height ~ bottom-bb*height  (lb = left blank)
//直线映射：[a1,a2]->[b1,b2] === 过点(a1,b1),(a2,b2)的直线上的一点(x,y)

float lb=40.0f/640.0f,
      rb=232.0f/640.0f,
      tb=32.0f/480.0f,
      bb=32.0f/480.0f;
//640 * 480下
//左侧空白为32，上16，下16，右224
//width = 384
//heigh = 448

static float mapping(float a1, float a2, float b1, float b2, long a) {
    if (a <= a1) {
        return b1;
    }
    if (a >= a2) {
        return b2;
    }
    float k = (b2 - b1) / (a2 - a1);
    return k * (float(a) - a1) + b1;
}

void XYMapping(RECT R, POINT P, float* x, float* y) {
    float width = float(R.right - R.left);
    float height = float(R.bottom - R.top);
    *x = mapping(float(R.left) + lb * width, float(R.right) - rb * width, 8.0f, 376.0f, P.x);
    *y = mapping(float(R.top) + tb * height, float(R.bottom) - bb * height, 16.0f, 432.0f, P.y);
}

//修改游戏内坐标值
//x:th08.exe+13D61AC y:th08.exe+13D61B0 类型：单浮点

float x, y;
DWORD xAddr = 0x00400000+0x013D61AC,
      yAddr = 0x00400000+0x013D61B0;

int main()
{
    pid = GetProcessID(processName);
    if (pid)
    {
        hd = OpenProcess(PROCESS_ALL_ACCESS, FALSE, pid);  //进程句柄
        
        if (!hd)
        {
            printf("无法获取目标进程句柄！！");
            getchar();
            return 0;
        }
        printf("%d\n", pid);
        gameHandle = FindMainWindow(pid);
    }
    else {
        printf("无法获取目标进程PID！！");
    }
    while (1) { //主循环
        if (GetCursorPos(&mousePos) == 0) {
            printf("无法获取指针位置！\n");
            system("pause");
        }
        if (GetWindowRect(gameHandle, &gameWindow) == 0) {
            printf("无法获取窗口大小！\n");
            system("pause");
        }
        XYMapping(gameWindow, mousePos, &x, &y);
        if (WriteProcessMemory(hd, LPVOID(xAddr), &x, sizeof(float), 0) == 0) {
            printf("无法修改内存！\n");
            system("pause");
        }
        if (WriteProcessMemory(hd, LPVOID(yAddr), &y, sizeof(float), 0) == 0) {
            printf("无法修改内存！\n");
            system("pause");
        }
    }
    return 0;
}
