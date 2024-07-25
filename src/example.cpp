#include "macros/unwrap.hpp"
#if defined(_WIN32)
    #include "process-win.hpp"
#else
    #include "process.hpp"
#endif
namespace {
auto run(const int argc, const char* const argv[]) -> bool {
    assert_b(argc >= 1);
    auto args = std::vector<const char*>();
    for(auto i = 1; i < argc; i += 1) {
        args.push_back(argv[i]);
    }
    args.push_back(NULL);

    auto process = process::Process();
    assert_b(process.start(args));
    unwrap_ob(result, process.join());
    print("result:");
    print("  reason=", int(result.reason));
    print("  code=", result.code);
    print("  stdout=", result.out);
    print("  stderr=", result.err);
    return true;
}
} // namespace

auto main(const int argc, const char* const argv[]) -> int {
    return run(argc, argv) ? 0 : 1;
}
