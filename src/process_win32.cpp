#include "process.hpp"

#include <memory>
#include <functional>

#include <Windows.h>
#include <psapi.h>
#include <tlhelp32.h>

class handle_deleter {
public:
    handle_deleter(HANDLE handle) : handle_{handle} {}
    ~handle_deleter() {
        if (handle_ != NULL) {
            CloseHandle(handle_);
        }
    }

private:
    HANDLE handle_;
};

std::chrono::system_clock::time_point filetime_to_chrono(const FILETIME &ft) {
    using namespace std::chrono;

    SYSTEMTIME st;
    FileTimeToSystemTime(&ft, &st);

    const auto date = year{st.wYear} / month{st.wMonth} / day{st.wDay};
    const auto time = sys_days{date} + hours{st.wHour} + minutes{st.wMinute} + seconds{st.wSecond} + milliseconds{st.wMilliseconds};
    return time;
}

BOOL CALLBACK enum_windows_cb(HWND hwnd, LPARAM lparam) {
    const auto func = std::bit_cast<std::function<BOOL(HWND, LPARAM)> *>(lparam);
    return (*func)(hwnd, lparam);
}

int get_pid(HWND hwnd) {
    DWORD process_id;
    if (!GetWindowThreadProcessId(hwnd, &process_id)) {
        return -1;
    }
    return static_cast<int>(process_id);
}

HWND get_hwnd(int pid) {
    HWND                              result = NULL;
    std::function<BOOL(HWND, LPARAM)> cb     = [&result, pid](HWND hwnd, LPARAM lParam) -> BOOL {
        const int process_id = get_pid(hwnd);
        if (process_id != -1 && process_id == pid) {
            result = hwnd;
            return FALSE; // stop searching
        }
        return TRUE;
    };
    EnumWindows(enum_windows_cb, std::bit_cast<LPARAM>(&cb));
    return result;
}

#ifdef UNICODE
using tstring = std::wstring;

// https://stackoverflow.com/a/3999597
std::string utf8_encode(std::wstring_view wstr) {
    if (wstr.empty()) {
        return {};
    }
    const int   size_needed = WideCharToMultiByte(CP_UTF8, 0, wstr.data(), static_cast<int>(wstr.size()), NULL, 0, NULL, NULL);
    std::string result(size_needed, 0);
    WideCharToMultiByte(CP_UTF8, 0, wstr.data(), static_cast<int>(wstr.size()), result.data(), size_needed, NULL, NULL);
    return result;
}
#else
using tstring = std::string;

std::string utf8_encode(std::string_view str) {
    return std::string{str};
}
#endif

namespace apptime {
bool process::exist() const {
    const HANDLE   handle = OpenProcess(PROCESS_QUERY_INFORMATION, FALSE, process_id_);
    handle_deleter hd{handle};
    return handle != NULL;
}

std::string process::window_name() const {
    const HWND hwnd = get_hwnd(process_id_);
    if (hwnd == NULL) {
        return {};
    }

    // nvcontainer.exe doesn't allow SendMessage to be used, so a timer is set
    constexpr unsigned timeout = 100;
    DWORD_PTR          length;
    if (!SendMessageTimeout(hwnd, WM_GETTEXTLENGTH, 0, 0, SMTO_BLOCK, timeout, &length) || length <= 0) {
        return {};
    }
    tstring result(length + 1, '\0');
    if (!SendMessageTimeout(hwnd, WM_GETTEXT, length + 1, std::bit_cast<LPARAM>(result.data()), SMTO_BLOCK, timeout, &length) || length != result.size() - 1) {
        return {};
    }
    result.pop_back(); // remove the last \0
    return utf8_encode(result);
}

std::string process::full_path() const {
    const HANDLE   handle = OpenProcess(PROCESS_QUERY_INFORMATION | PROCESS_VM_READ, FALSE, process_id_);
    handle_deleter hd{handle};
    if (handle == NULL) {
        return {};
    }

    TCHAR path[MAX_PATH] = {};
    if (GetModuleFileNameEx(handle, NULL, path, MAX_PATH) != 0) {
        return utf8_encode(path);
    }
    return {};
}

std::chrono::system_clock::time_point process::start() const {
    HANDLE         handle = OpenProcess(PROCESS_QUERY_INFORMATION, FALSE, process_id_);
    handle_deleter hd{handle};
    if (handle == NULL) {
        return {};
    }

    FILETIME creation, exit, kernel, user;
    if (!GetProcessTimes(handle, &creation, &exit, &kernel, &user)) {
        return {};
    }
    return filetime_to_chrono(creation);
}

// static

std::vector<process> process::active_processes() {
    const HANDLE snapshot = CreateToolhelp32Snapshot(TH32CS_SNAPPROCESS, 0);
    if (snapshot == INVALID_HANDLE_VALUE) {
        return {};
    }
    handle_deleter       hd{snapshot};
    std::vector<process> result;

    PROCESSENTRY32 entry = {};
    entry.dwSize         = sizeof(PROCESSENTRY32);
    if (Process32First(snapshot, &entry)) {
        do {
            process p{static_cast<int>(entry.th32ProcessID)};
            if (p.exist()) {
                result.push_back(p);
            }
        } while (Process32Next(snapshot, &entry));
    }

    return result;
}

std::vector<process> process::active_windows() {
    std::vector<process>              result;
    std::function<BOOL(HWND, LPARAM)> cb = [&result](HWND hwnd, LPARAM lParam) -> BOOL {
        const int process_id = get_pid(hwnd);
        if (process_id != -1) {
            result.push_back(process{process_id});
        }
        return TRUE;
    };
    EnumWindows(enum_windows_cb, std::bit_cast<LPARAM>(&cb));
    return result;
}

process process::focused_window() {
    const HWND hwnd = GetForegroundWindow();
    if (!hwnd) {
        return process{-1};
    }

    static process last_focused{-1};
    const int      pid = get_pid(hwnd);
    if (pid != last_focused.process_id_) {
        last_focused                = process{pid};
        last_focused.focused_start_ = std::chrono::system_clock::now();
    }
    return last_focused;
}
} // namespace apptime