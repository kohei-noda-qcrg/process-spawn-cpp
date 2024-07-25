#pragma once
#include <functional>
#include <span>

#include "util/fd.hpp"

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

    ExitReason reason;
    int        code;
};

class Process {
  private:
    struct FDPair {
        FileDescriptor output;
        FileDescriptor input;
    };

    pid_t  pid    = 0;
    Status status = Status::Init;
    FDPair pipes[3];

  public:
    std::function<OnOutput> on_stdout;
    std::function<OnOutput> on_stderr;

    // argv.back() and env.back() must be NULL
    auto start(std::span<const char* const> argv, std::span<const char* const> env = {}, const char* workdir = nullptr) -> bool;
    auto join(bool force = false) -> std::optional<Result>;
    auto get_pid() const -> pid_t;
    auto get_stdin() -> FileDescriptor&;
    auto get_status() const -> Status;
    auto collect_outputs() -> bool;
};
} // namespace process
