#include "vm.hpp"
#include <iostream>
using namespace vm::runtime;

Object::Object(Type type, const byte *data, std::size_t data_size)
    : type(type), data(nullptr), data_size(data_size), links(0)
{
    if (type == Type::ARRAY)
    {
        this->data = new byte[data_size * sizeof(Link)];
        new (this->data) Link[data_size];
    }
    else
    {
        this->data = new byte[data_size];
        std::copy(data, data + data_size, this->data);
    }
}

bool vm::runtime::Object::operator==(const Object &other) const
{
    return type == other.type &&
           data_size == other.data_size &&
           std::equal(data, data + data_size, other.data);
}

bool vm::runtime::Object::operator!=(const Object &other) const
{
    return !(*this == other);
}

bool vm::runtime::Object::operator<=(const Object &other) const
{
    switch (type)
    {
    case runtime::Type::I32:
        return static_cast<int>(*this) <= static_cast<int>(other);
    case runtime::Type::USIZE:
        return static_cast<u32>(*this) <= static_cast<u32>(other);
    default:
        return static_cast<std::string>(*this) <= static_cast<std::string>(other);
    }
}

bool vm::runtime::Object::operator>=(const Object &other) const
{
    switch (type)
    {
    case runtime::Type::I32:
        return static_cast<int>(*this) >= static_cast<int>(other);
    case runtime::Type::USIZE:
        return static_cast<u32>(*this) >= static_cast<u32>(other);
    default:
        return static_cast<std::string>(*this) >= static_cast<std::string>(other);
    }
}

bool vm::runtime::Object::operator<(const Object &other) const
{
    return !(*this >= other);
}

bool vm::runtime::Object::operator>(const Object &other) const
{
    return !(*this <= other);
}

Object::operator bool() const
{
    for (std::size_t i = 0; i < data_size; ++i)
    {
        if (data[i])
        {
            return true;
        }
    }
    return false;
}

vm::runtime::Object::operator int() const
{
    return reinterpret_cast<int *>(data)[0];
}

vm::runtime::Object::operator u32() const
{
    return reinterpret_cast<u32 *>(data)[0];
}

vm::runtime::Object::operator std::string() const
{
    std::string result;
    switch (type)
    {
    case Type::I32:
    {
        result = std::to_string(static_cast<int>(*this));
        break;
    }
    case Type::USIZE:
    {
        result = std::to_string(static_cast<u32>(*this));
        break;
    }
    case Type::STRING:
    {
        for (std::size_t i = 0; i < data_size; ++i)
        {
            result.push_back(data[i]);
        }
        break;
    }
    case Type::ARRAY:
    {
        result = "[";
        for (std::size_t i = 0; i < data_size; ++i)
        {
            Link *links = reinterpret_cast<Link *>(data);
            if (i > 0)
                result += ", ";
            if (links[i].object == nullptr)
            {
                result += "...";
                break;
            }
            result += static_cast<std::string>(*(links[i].object));
        }
        result.push_back(']');
        break;
    }
    default:
    {
        result = std::to_string(reinterpret_cast<std::size_t>(this));
        break;
    }
    }
    return result;
}

Object::~Object()
{
    delete[] data;
}

vm::runtime::Link::Link()
    : object(nullptr) {}

Link &vm::runtime::Link::operator=(Object *&obj)
{
    if (object != nullptr)
        --object->links;

    object = obj;
    ++object->links;
    return *this;
}

Link::~Link()
{
    if (object != nullptr)
    {
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
    if (this != &other)
    {
        std::swap(size, other.size);
        std::swap(variables, other.variables);
    }
    return *this;
}

GlobalVariables::~GlobalVariables()
{
    if (variables != nullptr)
    {
        delete[] variables;
    }
}