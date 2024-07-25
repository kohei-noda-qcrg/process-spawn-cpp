#pragma once
#include <functional>
#include <span>
#include <string>
#include <optional>
#include <vector>
#include <windows.h>

namespace process {
using OnOutput = void(std::span<char> output);

enum class Status : int {
    Init,
    Running,
    Finished,
    Joined,
};

struct Result {
    enum class ExitReason : int {
        Exit,
        Signal,
    };

    ExitReason  reason;
    int         code;
};

class Process {
  private:
    struct PipePair {
        HANDLE output;
        HANDLE input;
    };

    HANDLE              process_handle = nullptr;
    HANDLE              thread_handle  = nullptr;
    Status              status = Status::Init;
    PipePair            pipes[3];
    bool                failed = false;

  public:
    std::function<OnOutput> on_stdout;
    std::function<OnOutput> on_stderr;

    // argv.back() and env.back() must be NULL
    auto start(std::span<const char* const> argv, std::span<const char* const> env = {}, const char* workdir = nullptr) -> bool;
    auto join(bool force = false) -> std::optional<Result>;
    auto get_pid() const -> DWORD;
    auto get_stdin() -> HANDLE&;
    auto get_status() const -> Status;
    auto collect_outputs() -> bool;
};
} // namespace process
