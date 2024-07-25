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

    return true;
}

auto Process::join(const bool force) -> std::optional<Result> {
    assert_o(status == Status::Running || status == Status::Finished);
    status = Status::Joined;
    assert_o(!force || TerminateProcess(process_handle, 1), "failed to kill process");


    DWORD exit_code;
    GetExitCodeProcess(process_handle, &exit_code);

    CloseHandle(process_handle);
    CloseHandle(thread_handle);

    for (int i = 0; i < 3; i += 1) {
        CloseHandle(pipes[i].output);
    }

    return Result{
        .reason = exit_code == 0 ? Result::ExitReason::Exit : Result::ExitReason::Signal,
        .code = (int)exit_code,
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

auto Process::collect_outputs() -> bool {
    HANDLE handles[] = { pipes[1].output, pipes[2].output }; 
    DWORD wait_result, wait_process;
    
    wait_process = WaitForSingleObject(process_handle, 0);
    if (wait_process == WAIT_OBJECT_0){
        status = Status::Finished;
    }
    wait_result = WaitForMultipleObjects(2, handles, FALSE, 0);
    for (int i = 0; i < 2; i += 1) {
        DWORD len;
        std::array<char, 256> buf;
        while (true) {
            DWORD bytes_avail = 0;
            auto success = PeekNamedPipe(handles[i], NULL, 0, NULL, &bytes_avail, NULL);
            if (!success || bytes_avail == 0) {
                break;
            }
            auto res = ReadFile(handles[i], buf.data(), buf.size(), &len, NULL);
            if (!res || len <= 0) {
                break;
            }
            auto callback = i == 0 ? on_stdout : on_stderr;
            if (callback) {
                callback({buf.data(), size_t(len)});
            }
        }
    }    
    return true;
}
} // namespace process
