#include "vm.hpp"
#include <climits>
#include <stdexcept>
#include <iostream>

using namespace vm::code;

Reader::Reader(const fs::path &path)
    : _input(path, std::ios::binary), _buffer(new byte[DEFAULT_BUFFER_SIZE]), _capacity(DEFAULT_BUFFER_SIZE), _limit(0), _pos(0)
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
      _pos(other._pos)
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

        other._buffer = nullptr;
        other._capacity = 0;
        other._limit = 0;
        other._pos = 0;
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

Header Reader::read_header() {
    return {read_32(), read_16(), read_16()};
}

static void put(byte *&data, byte id, byte *value, u16 value_size)
{
    data = new byte[1 + value_size];
    std::copy(&id, &id + 1, data);
    std::copy(value, value + value_size, data + 1);
}

ConstantPool Reader::read_constants() {
    u16 size = read_16();
    byte **data = new byte*[size];
    for (u16 i = 0; i < size; ++i) {
        byte id = read_byte();
        switch (id)
        {
        case 0x01:
        case 0x02: {
            u32 value = read_32();
            put(data[i], id, reinterpret_cast<byte *>(&value), 4);
            break;
        }
        case 0x03: {
            u16 length = read_16();
            byte *value = new byte[length + 2];
            read_bytes(value + 2, length);
            std::copy(&length, &length + 2, value);
            put(data[i], id, value, 2 + length);
            break;
        }
        default:
            throw InvalidBytecodeException("Unexpected type in constant pool");
        }
    }
    return {size, data};
}

void Reader::close() {
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

byte Reader::read_byte()
{
    refill_buffer();

    if (_pos >= _limit)
    {
        throw std::out_of_range("No more bytes to read!");
    }

    return _buffer[_pos++];
}