#include "vm.hpp"

using namespace vm;

static void check_header(const code::Header &header) {
    if (header.magic != 0x534E4131U) {
        throw code::InvalidBytecodeException("Magic constant is invalid!");
    }
    if (header.version) {
    }
    if (header.main_function_index < 0) {
        throw code::InvalidBytecodeException("Cannot run bytecode without a main function");
    }
}

void proccess(const fs::path &file)
{
    code::Reader reader(file);
    code::Header header = reader.read_header();
    check_header(header);
    code::ConstantPool pool = reader.read_constants();
}