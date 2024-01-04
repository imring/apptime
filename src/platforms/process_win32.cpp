#include "process.hpp"

#include <functional>
#include <memory>

#include <Windows.h>
#include <psapi.h>
#include <tlhelp32.h>

#include "encoding.hpp"

class handle_deleter {
public:
    explicit handle_deleter(HANDLE handle) : handle_{handle} {}
    ~handle_deleter() {
        if (handle_ != nullptr) {
            CloseHandle(handle_);
        }
    }

private:
    HANDLE handle_;
};

std::chrono::system_clock::time_point filetime_to_chrono(const FILETIME &filetime) {
    using namespace std::chrono;

    SYSTEMTIME st;
    FileTimeToSystemTime(&filetime, &st);

    const auto date = year{st.wYear} / month{st.wMonth} / day{st.wDay};
    const auto time = sys_days{date} + hours{st.wHour} + minutes{st.wMinute} + seconds{st.wSecond} + milliseconds{st.wMilliseconds};
    return time;
}

// util function that allows to use lambda-expressions in EnumWindows
BOOL CALLBACK enum_windows_cb(HWND hwnd, LPARAM lparam) {
    const auto *func = std::bit_cast<std::function<BOOL(HWND, LPARAM)> *>(lparam);
    return (*func)(hwnd, lparam);
}

int get_pid(HWND hwnd) {
    DWORD process_id; // NOLINT(cppcoreguidelines-init-variables)
    if (!GetWindowThreadProcessId(hwnd, &process_id)) {
        return -1;
    }
    return static_cast<int>(process_id);
}

HWND get_hwnd(int pid) {
    HWND                                    result   = nullptr;
    const std::function<BOOL(HWND, LPARAM)> callback = [&result, pid](HWND hwnd, LPARAM /*lParam*/) -> BOOL {
        const int process_id = get_pid(hwnd);
        if (process_id != -1 && process_id == pid) {
            result = hwnd;
            return FALSE; // stop searching
        }
        return TRUE;
    };
    EnumWindows(enum_windows_cb, std::bit_cast<LPARAM>(&callback));
    return result;
}

namespace apptime {
bool process::exist() const {
    HANDLE               handle = OpenProcess(PROCESS_QUERY_INFORMATION, FALSE, process_id_);
    const handle_deleter hd{handle};
    return handle != nullptr;
}

std::string process::window_name() const {
    HWND hwnd = get_hwnd(process_id_);
    if (hwnd == nullptr) {
        return {};
    }

    // nvcontainer.exe doesn't allow SendMessage to be used, so a timer is set
    constexpr unsigned timeout = 100;
    DWORD_PTR          length; // NOLINT(cppcoreguidelines-init-variables)
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
    HANDLE               handle = OpenProcess(PROCESS_QUERY_INFORMATION | PROCESS_VM_READ, FALSE, process_id_);
    const handle_deleter hd{handle};
    if (handle == nullptr) {
        return {};
    }

    TCHAR path[MAX_PATH] = {};
    if (GetModuleFileNameEx(handle, nullptr, path, MAX_PATH) != 0) {
        return utf8_encode(path);
    }
    return {};
}

std::chrono::system_clock::time_point process::start() const {
    HANDLE               handle = OpenProcess(PROCESS_QUERY_INFORMATION, FALSE, process_id_);
    const handle_deleter hd{handle};
    if (handle == nullptr) {
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
    HANDLE snapshot = CreateToolhelp32Snapshot(TH32CS_SNAPPROCESS, 0);
    if (snapshot == INVALID_HANDLE_VALUE) {
        return {};
    }
    const handle_deleter hd{snapshot};
    std::vector<process> result;

    PROCESSENTRY32 entry = {};
    entry.dwSize         = sizeof(PROCESSENTRY32);
    for (BOOL found = Process32First(snapshot, &entry); found; found = Process32Next(snapshot, &entry)) {
        const process proc{static_cast<int>(entry.th32ProcessID)};
        if (proc.exist()) {
            result.push_back(proc);
        }
    }

    return result;
}

std::vector<process> process::active_windows(bool only_visible) {
    std::vector<process>                    result;
    const std::function<BOOL(HWND, LPARAM)> callback = [&result, only_visible](HWND hwnd, LPARAM /*lParam*/) -> BOOL {
        if (only_visible && !IsWindowVisible(hwnd)) {
            return TRUE;
        }

        const int process_id = get_pid(hwnd);
        if (process_id != -1) {
            result.emplace_back(process_id);
        }
        return TRUE;
    };
    EnumWindows(enum_windows_cb, std::bit_cast<LPARAM>(&callback));
    return result;
}

process process::focused_window() {
    HWND hwnd = GetForegroundWindow();
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