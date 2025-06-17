#include <iostream>
#include <format>
#include <fstream>
#include <filesystem>

#include "vm.hpp"

constexpr static const char *INVALID_ARGUMENTS = "Invalid arguments for ShellVM\n";
constexpr static const char *USAGE = "\
shellvm [OPTIONS] file_to_run \n\
  OPTIONS \n\
    -d, --debug : Run VM in debug configuration";

int main(int argc, char **argv)
{
    if (argc < 2)
    {
        std::cerr << std::format("{}\n{}\n{}", INVALID_ARGUMENTS, "Usage:", USAGE);
        return EXIT_FAILURE;
    }

    bool debug_mode = false;
    if (argc > 2 && (!std::strcmp("-d", argv[1]) || !strcmp("--debug", argv[1])))
        debug_mode = true;

    fs::path target(argv[argc - 1]);

    if (!fs::exists(target))
    {
        std::cerr << INVALID_ARGUMENTS << target << " wasn't found";
        return EXIT_FAILURE;
    }

    vm::process(target, debug_mode);
}