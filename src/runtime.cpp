#include "vm.hpp"

using namespace vm::runtime;

Object::Object(Type type, byte *data, std::size_t data_size) 
    : type(type), data(new byte[data_size]), data_size(data_size), links(0)
{
    std::copy(data, data + data_size, this->data);
}

bool vm::runtime::Object::operator==(const Object &other) const
{
    return type == other.type && 
           data_size == other.data_size && 
           std::equal(data, data + data_size, other.data); 
}

Object::operator bool() const {
    for (std::size_t i = 0; i < data_size; ++i) {
        if (data[i]) {
            return true;
        }
    }
    return false;
}

Object::~Object() 
{
    delete[] data;
}

Link &vm::runtime::Link::operator=(Object *&obj)
{
    if (object != nullptr) 
        --object->links;

    object = obj;
    ++object->links;    
}

Link::~Link()
{
    if (object != nullptr) {
        --object->links;
    }
}

GlobalVariables::GlobalVariables(u16 size, Link *variables)
    : size(size), variables(variables) {}

vm::runtime::GlobalVariables::GlobalVariables(GlobalVariables &&other)
    : size(other.size), variables(other.variables)
{
    other.variables = nullptr;
}

GlobalVariables &vm::runtime::GlobalVariables::operator=(GlobalVariables &&other)
{
    if (this != &other) {
        std::swap(size, other.size);
        std::swap(variables, other.variables);
    }
    return *this;
}

GlobalVariables::~GlobalVariables()
{
    if (variables != nullptr) {
        for (u16 i = 0; i < size; ++i) {
            delete variables[i].object;
        }
        delete[] variables;
    }
}