#include "vm.hpp"
#include <stdexcept>
#include <cstdint>
#include <algorithm>
#include <iostream>

using namespace vm::code;

ConstantPool::ConstantPool(u16 size, runtime::Object **data)
    : size(size), data(data) {}

ConstantPool::ConstantPool(ConstantPool &&other)
    : size(other.size), data(other.data)
{
    other.data = nullptr;
}

ConstantPool &ConstantPool::operator=(ConstantPool &&other)
{
    if (this != &other)
    {
        std::swap(this->size, other.size);
        std::swap(this->data, other.data);
    }
    return *this;
}

ConstantPool::~ConstantPool()
{
    if (data != nullptr)
    {
        for (int i = 0; i < size; ++i)
        {
            delete data[i];
        }
        delete[] data;
    }
}

FunctionTable::FunctionTable(u16 size, Function *functions)
    : size(size), functions(functions) {}

FunctionTable::FunctionTable(FunctionTable &&other)
    : size(other.size), functions(other.functions)
{
    other.functions = nullptr;
}

FunctionTable &FunctionTable::operator=(FunctionTable &&other)
{
    if (this != &other)
    {
        std::swap(this->size, other.size);
        std::swap(this->functions, other.functions);
    }
    return *this;
}

FunctionTable::~FunctionTable()
{
    if (functions != nullptr)
    {
        delete[] functions;
    }
}

IntrinsicTable::IntrinsicTable(u16 size, Intrinsic *functions)
    : size(size), functions(functions) {}

IntrinsicTable::IntrinsicTable(IntrinsicTable &&other)
    : size(other.size), functions(other.functions)
{
    other.functions = nullptr;
}

IntrinsicTable &IntrinsicTable::operator=(IntrinsicTable &&other)
{
    if (this != &other)
    {
        std::swap(this->size, other.size);
        std::swap(this->functions, other.functions);
    }
    return *this;
}

IntrinsicTable::~IntrinsicTable()
{
    if (functions != nullptr)
    {
        delete[] functions;
    }
}
