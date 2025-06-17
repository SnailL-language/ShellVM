#include "vm.hpp"
#include <climits>
#include <stdexcept>
#include <iostream>

using namespace vm::code;

Reader::Reader(const fs::path &path)
    : _input(path, std::ios::binary), _buffer(new byte[DEFAULT_BUFFER_SIZE]), _capacity(DEFAULT_BUFFER_SIZE), _limit(0), _pos(0), _absolute_pos(0)
{
    if (!_input.is_open())
    {
        throw std::runtime_error("File opening failed");
    }
}

Reader::Reader(Reader &&other)
    : _input(std::move(other._input)),
      _buffer(other._buffer),
      _capacity(other._capacity),
      _limit(other._limit),
      _pos(other._pos),
      _absolute_pos(other._absolute_pos)
{
    other._buffer = nullptr;
    other._capacity = 0;
    other._limit = 0;
    other._pos = 0;
}

Reader &Reader::operator=(Reader &&other)
{
    if (this != &other)
    {
        _input = std::move(other._input);
        delete[] _buffer;
        _buffer = other._buffer;
        _capacity = other._capacity;
        _limit = other._limit;
        _pos = other._pos;
        _absolute_pos = other._absolute_pos;

        other._buffer = nullptr;
        other._capacity = 0;
        other._limit = 0;
        other._pos = 0;
        other._absolute_pos = 0;
    }
    return *this;
}

u16 Reader::read_16()
{
    u16 value;
    read_big_endian(reinterpret_cast<byte *>(&value), 2);
    return value;
}

u32 Reader::read_32()
{
    u32 value;
    read_big_endian(reinterpret_cast<byte *>(&value), 4);
    return value;
}

void vm::code::Reader::read_big_endian(byte *dest, std::size_t size)
{
    for (std::size_t i = 1; i <= size; ++i)
    {
        dest[size - i] = read_byte();
    }
}

Header Reader::read_header()
{
    return {read_32(), read_16(), read_16()};
}

static vm::runtime::Type to_type(byte id)
{
    switch (id)
    {
    case 0x00:
        return vm::runtime::Type::VOID;
    case 0x01:
        return vm::runtime::Type::I32;
    case 0x02:
        return vm::runtime::Type::USIZE;
    case 0x03:
        return vm::runtime::Type::STRING;
    case 0x04:
        return vm::runtime::Type::ARRAY;
    default:
        throw std::invalid_argument("Unknown type byte");
    }
}

ConstantPool Reader::read_constants()
{
    u16 size = read_16();
    runtime::Object **data = new runtime::Object *[size];
    for (u16 i = 0; i < size; ++i)
    {
        byte id = read_byte();
        switch (id)
        {
        case 0x01:
        case 0x02:
        {
            u32 value = read_32();
            byte *value_ptr = new byte[4];
            std::copy(reinterpret_cast<byte *>(&value), reinterpret_cast<byte *>(&value) + 4, value_ptr);
            data[i] = new runtime::Object(to_type(id), value_ptr, 4);
            break;
        }
        case 0x03:
        {
            u16 length = read_16();
            byte *value = new byte[length];
            read_bytes(value, length);
            // std::copy(&length, &length + 1, value);
            data[i] = new runtime::Object(to_type(id), value, length);
            break;
        }
        default:
            throw InvalidBytecodeException("Unexpected type in constant pool");
        }
    }
    return {size, data};
}

vm::runtime::GlobalVariables Reader::read_globals()
{
    u16 size = read_16();
    vm::runtime::Link *variables = new vm::runtime::Link[size];
    for (u16 i = 0; i < size; ++i)
    {
        byte name_length = read_byte();
        skip(name_length);
        byte id = read_byte();
        if (id == 0x04)
        {
            skip(5);
        }
    }
    return {size, variables};
}

FunctionTable vm::code::Reader::read_functions()
{
    u16 size = read_16();
    Function *functions = new Function[size];
    for (u16 i = 0; i < size; ++i)
    {
        byte name_length = read_byte();
        skip(name_length);
        functions[i].arg_count = read_byte();
        functions[i].return_type = to_type(read_byte());
        functions[i].local_count = read_16();
        functions[i].length = read_32();
        functions[i].offset = get_offset();
        skip(functions[i].length);
    }

    return {size, functions};
}

IntrinsicTable vm::code::Reader::read_intrinsics()
{
    u16 size = read_16();
    Intrinsic *functions = new Intrinsic[size];
    for (u16 i = 0; i < size; ++i)
    {
        byte name_length = read_byte();
        for (byte j = 0; j < name_length; ++j)
        {
            functions[i].name.push_back(read_byte());
        }
        functions[i].arg_count = read_byte();
        functions[i].return_type = to_type(read_byte());
    }

    return {size, functions};
}

void vm::code::Reader::skip(std::size_t delta)
{
    for (std::size_t i = 0; i < delta; ++i)
    {
        read_byte();
    }
}

void Reader::close()
{
    if (_input.is_open())
    {
        _input.close();
    }
}

Reader::~Reader()
{
    if (_buffer)
    {
        delete[] _buffer;
    }
    close();
}

void Reader::refill_buffer()
{
    if (_pos < _limit)
    {
        return;
    }

    _absolute_pos += _limit;
    _input.read(reinterpret_cast<char *>(_buffer), 8 * _capacity / CHAR_BIT);
    std::streamsize bytes_read = _input.gcount();

    if (bytes_read <= 0)
    {
        _limit = 0;
        return;
    }

    _limit = static_cast<std::size_t>(bytes_read);
    _pos = 0;
}

void Reader::read_bytes(byte *dest, std::size_t size)
{
    for (std::size_t i = 0; i < size; ++i)
    {
        dest[i] = read_byte();
    }
}

std::size_t vm::code::Reader::get_offset() const
{
    return _absolute_pos + _pos;
}

void vm::code::Reader::set_offset(std::size_t pos)
{
    _input.clear();
    _input.seekg(pos, std::ios_base::beg);
    _pos = _limit;
    refill_buffer();
    _absolute_pos = pos;
}

vm::runtime::Type vm::code::Reader::read_type()
{
    return to_type(read_byte());
}

byte Reader::read_byte()
{
    refill_buffer();

    if (_pos >= _limit)
    {
        throw std::out_of_range("No more bytes to read!");
    }
    return _buffer[_pos++];
}