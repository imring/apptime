#include "process_system.hpp"

#include <array>
#include <functional>

#include <Windows.h>
#include <psapi.h>
#include <tlhelp32.h>

#include "platforms/encoding.hpp"

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
using enum_windows_t = std::function<BOOL(HWND)>;
BOOL CALLBACK enum_windows_cb(HWND hwnd, LPARAM lparam) {
    const auto *func = std::bit_cast<enum_windows_t *>(lparam);
    return (*func)(hwnd);
}

int get_pid(HWND hwnd) {
    DWORD process_id; // NOLINT(cppcoreguidelines-init-variables)
    if (!GetWindowThreadProcessId(hwnd, &process_id)) {
        return -1;
    }
    return static_cast<int>(process_id);
}

std::vector<HWND> get_hwnds(int pid) {
    std::vector<HWND>    result;
    const enum_windows_t callback = [&result, pid](HWND hwnd) -> BOOL {
        const int process_id = get_pid(hwnd);
        if (process_id != -1 && process_id == pid) {
            result.push_back(hwnd);
        }
        return TRUE;
    };
    EnumWindows(enum_windows_cb, std::bit_cast<LPARAM>(&callback));
    return result;
}

namespace apptime {
bool process_system::exist() const {
    HANDLE               handle = OpenProcess(PROCESS_QUERY_INFORMATION, FALSE, process_id_);
    const handle_deleter hd{handle};
    return handle != nullptr;
}

std::string process_system::window_name() const {
    const std::vector<HWND> hwnds = get_hwnds(process_id_);
    if (hwnds.empty()) {
        return {};
    }

    // see all windows linked to this process id
    for (HWND hwnd: hwnds) {
        // nvcontainer.exe doesn't allow SendMessage to be used, so a timer is set
        constexpr unsigned timeout = 100;
        DWORD_PTR          length; // NOLINT(cppcoreguidelines-init-variables)
        if (!SendMessageTimeout(hwnd, WM_GETTEXTLENGTH, 0, 0, SMTO_BLOCK, timeout, &length) || length <= 0) {
            continue;
        }
        tstring result(length + 1, '\0');
        if (!SendMessageTimeout(hwnd, WM_GETTEXT, length + 1, std::bit_cast<LPARAM>(result.data()), SMTO_BLOCK, timeout, &length) ||
            length != result.size() - 1) {
            continue;
        }
        result.pop_back(); // remove the last \0
        return utf8_encode(result);
    }
    return {};
}

std::string process_system::full_path() const {
    HANDLE               handle = OpenProcess(PROCESS_QUERY_INFORMATION | PROCESS_VM_READ, FALSE, process_id_);
    const handle_deleter hd{handle};
    if (handle == nullptr) {
        return {};
    }

    std::array<TCHAR, MAX_PATH> path = {};
    if (GetModuleFileNameEx(handle, nullptr, path.data(), path.size()) != 0) {
        return utf8_encode(path.data());
    }
    return {};
}

std::chrono::system_clock::time_point process_system::start() const {
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

std::chrono::system_clock::time_point process_system::focused_start() const {
    return focused_start_;
}

// static

std::vector<process_system_mgr::process_type> process_system_mgr::active_processes() {
    HANDLE snapshot = CreateToolhelp32Snapshot(TH32CS_SNAPPROCESS, 0);
    if (snapshot == INVALID_HANDLE_VALUE) {
        return {};
    }
    const handle_deleter      hd{snapshot};
    std::vector<process_type> result;

    PROCESSENTRY32 entry = {};
    entry.dwSize         = sizeof(PROCESSENTRY32);
    for (BOOL found = Process32First(snapshot, &entry); found; found = Process32Next(snapshot, &entry)) {
        const process_system proc{static_cast<int>(entry.th32ProcessID)};
        if (proc.exist()) {
            result.emplace_back(std::make_unique<process_system>(proc));
        }
    }

    return result;
}

std::vector<process_system_mgr::process_type> process_system_mgr::active_windows(bool only_visible) {
    std::vector<process_type> result;
    const enum_windows_t      callback = [&result, only_visible](HWND hwnd) -> BOOL {
        if (only_visible && !IsWindowVisible(hwnd)) {
            return TRUE;
        }

        const int process_id = get_pid(hwnd);
        if (process_id != -1) {
            result.push_back(std::make_unique<process_system>(process_id));
        }
        return TRUE;
    };
    EnumWindows(enum_windows_cb, std::bit_cast<LPARAM>(&callback));
    return result;
}

process_system_mgr::process_type process_system_mgr::focused_window() {
    HWND hwnd = GetForegroundWindow();
    if (!hwnd) {
        return std::make_unique<process_system>(-1);
    }

    static process_system last_focused{-1};
    const int             pid = get_pid(hwnd);
    if (pid != last_focused.process_id_) {
        last_focused                = process_system{pid};
        last_focused.focused_start_ = std::chrono::system_clock::now();
    }
    return std::make_unique<process_system>(last_focused);
}
} // namespace apptime