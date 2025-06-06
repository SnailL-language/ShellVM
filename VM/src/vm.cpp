#include "vm.hpp"
#include <stdexcept>
#include <cstdint>
#include <algorithm>
#include <iostream>

using namespace vm::code;

static void check_header(const Header &header) {
    if (header.magic != 0x534E4131U) {
        throw InvalidBytecodeException("Magic constant is invalid!");
    }
    if (header.version) {
    }
    if (header.main_function_index < 0) {
        throw InvalidBytecodeException("Cannot run bytecode without a main function");
    }
}

void vm::proccess(const fs::path &file)
{
    Reader reader(file);
    Header header = reader.read_header();
    check_header(header);
    ConstantPool pool = reader.read_constants();
}

ConstantPool::ConstantPool(u16 size, byte **data)
    : size(size) , data(data) {}

ConstantPool::ConstantPool(ConstantPool &&other)
    : size(other.size), data(other.data)
{
    other.data = nullptr;
}

ConstantPool &ConstantPool::operator=(ConstantPool &&other)
{
    if (this != &other) {
        this->size = other.size;
        std::swap(this->data, other.data);
    }
    return *this;
}

ConstantPool::~ConstantPool()
{
    if (data != nullptr) {
        for (int i = 0; i < size; ++i) {
            delete[] data[i];
        }
        delete[] data;
    }
}
