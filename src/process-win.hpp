#pragma once
#include <span>
#include <string>
#include <thread>
#include <optional>
#include <vector>
#include <windows.h>

namespace process {
enum class Status : int {
    Init,
    Running,
    Joined,
};

struct Result {
    enum class ExitReason : int {
        Exit,
        Signal,
    };

    ExitReason  reason;
    int         code;
    std::string out;
    std::string err;
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
    std::string         outputs[2];
    std::thread         output_collector;
    HANDLE              output_collector_event = nullptr;

    auto output_collector_main() -> void;
    auto collect_outputs() -> void;

  public:
    // argv.back() and env.back() must be NULL
    auto start(std::span<const char* const> argv, std::span<const char* const> env = {}, const char* workdir = nullptr) -> bool;
    auto join(bool force = false) -> std::optional<Result>;
    auto get_pid() const -> DWORD;
    auto get_stdin() -> HANDLE&;
    auto get_status() const -> Status;
};
} // namespace process
