#include <array>
#include <iostream>
#include <wtypesbase.h>

#include "process-win.hpp"
#include "macros/assert.hpp"

namespace process {

auto create_pipe(HANDLE& read_pipe, HANDLE& write_pipe) -> bool {
    SECURITY_ATTRIBUTES saAttr;
    saAttr.nLength = sizeof(SECURITY_ATTRIBUTES);
    saAttr.bInheritHandle = TRUE;
    saAttr.lpSecurityDescriptor = NULL;
    if (!CreatePipe(&read_pipe, &write_pipe, &saAttr, 0)) {
        return false;
    }
    if (!SetHandleInformation(read_pipe, HANDLE_FLAG_INHERIT, 0)) {
        return false;
    }
    return true;
}

auto Process::start(const std::span<const char* const> argv, const std::span<const char* const> env, const char* const workdir) -> bool {
    assert_b(status == Status::Init);
    assert_b(!argv.empty());
    assert_b(argv.back() == NULL);
    assert_b(env.empty() || env.back() == NULL);
    status = Status::Running;

    for (int i = 0; i < 3; i += 1) {
        if (!create_pipe(pipes[i].output, pipes[i].input)) {
            return false;
        }
    }
    std::cout << "create pipe success" << std::endl;
    STARTUPINFO si;
    PROCESS_INFORMATION pi;
    ZeroMemory(&si, sizeof(si));
    si.cb = sizeof(si);
    si.hStdInput = pipes[0].output;
    si.hStdOutput = pipes[1].input;
    si.hStdError = pipes[2].input;
    si.dwFlags |= STARTF_USESTDHANDLES;

    ZeroMemory(&pi, sizeof(pi));

    std::string command_line;
    for (size_t i = 0; argv[i] != nullptr; i += 1) {
        command_line += argv[i];
        command_line += " ";
    }
    std::cout << "command_line: " << command_line << std::endl;
    if (!CreateProcess(
            NULL,
            (LPSTR)command_line.data(),
            NULL,
            NULL,
            TRUE,
            0,
            NULL,
            (LPCSTR)workdir,
            &si,
            &pi)) {
        std::cerr << "CreateProcess failed" << std::endl;
        return false;
    }

    process_handle = pi.hProcess;
    thread_handle = pi.hThread;

    output_collector_event = CreateEvent(NULL, TRUE, FALSE, NULL);
    if (output_collector_event == NULL) {
        return false;
    }

    output_collector = std::thread(&Process::output_collector_main, this);
    return true;
}

auto Process::join(const bool force) -> std::optional<Result> {
    assert_o(status == Status::Running);
    status = Status::Joined;
    std::cout << "join" << std::endl;
    assert_o(!force || TerminateProcess(process_handle, 1), "failed to kill process");


    std::cout << "WaitForSingleObject" << std::endl;
    WaitForSingleObject(process_handle, INFINITE);

    std::cout << "GetExitCodeProcess" << std::endl;
    DWORD exit_code;
    GetExitCodeProcess(process_handle, &exit_code);

    std::cout << "output_collector_event notify" << std::endl;
    SetEvent(output_collector_event);
    std::cout << "output_collector join" << std::endl;

    output_collector.join();

    std::cout << "Close Process handle" << std::endl;
    CloseHandle(process_handle);
    std::cout << "Close Thread handle" << std::endl;
    CloseHandle(thread_handle);

    std::cout << "CloseHandle" << std::endl;
    for (int i = 0; i < 3; i += 1) {
        CloseHandle(pipes[i].output);
        std::cout << "pipe: " << i << " closed" << std::endl;
    }

    return Result{
        .reason = exit_code == 0 ? Result::ExitReason::Exit : Result::ExitReason::Signal,
        .code = (int)exit_code,
        .out = std::move(outputs[0]),
        .err = std::move(outputs[1]),
    };
}

auto Process::get_pid() const -> DWORD {
    return GetProcessId(process_handle);
}

auto Process::get_stdin() -> HANDLE& {
    return pipes[0].input;
}

auto Process::get_status() const -> Status {
    return status;
}

auto Process::output_collector_main() -> void {
    HANDLE handles[] = { pipes[1].output, pipes[2].output, output_collector_event };
    DWORD wait_result;
    while (true) {
        wait_result = WaitForMultipleObjects(3, handles, TRUE, 0);
        std::cout << "output_collector_main, wait_result: " << wait_result << std::endl;
        if (wait_result >= WAIT_OBJECT_0 + 2) {
            break;
        }
        for (int i = 0; i < 2; i += 1) {
            if (wait_result == WAIT_OBJECT_0 + i) {
                DWORD len;
                std::array<char, 256> buf;
                while (true) {
                    if (!ReadFile(handles[i], buf.data(), buf.size(), &len, NULL) || len <= 0) {
                        break;
                    }
                    outputs[i].insert(outputs[i].end(), buf.begin(), buf.begin() + len);
                    if(size_t(len) < buf.size()) {
                        break;
                    }
                }
            }
        }
    }
    CloseHandle(handles[0]);
    CloseHandle(handles[1]);
}

} // namespace process
