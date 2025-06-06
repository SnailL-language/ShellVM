#include "vm.hpp"
#include <climits>

vm::Reader::Reader(const fs::path &path)
    : _input(path, std::ios::binary), _capacity(DEFAULT_BUFFER_SIZE), _buffer(new byte[DEFAULT_BUFFER_SIZE]), _limit(0), _pos(0)
{
    if (!_input.is_open())
    {
        throw std::runtime_error("File opening failed");
    }
}

vm::Reader::Reader(Reader &&other)
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

vm::Reader &vm::Reader::operator=(Reader &&other)
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

bool vm::Reader::read_word(word &dest)
{
    return read_bytes(reinterpret_cast<byte *>(&dest), WORD_SIZE) == WORD_SIZE;
}

vm::Reader::~Reader()
{
    if (_buffer)
    {
        delete[] _buffer;
    }
    if (_input.is_open())
    {
        _input.close();
    }
}

void vm::Reader::refill_buffer()
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

std::size_t vm::Reader::read_bytes(byte *dest, std::size_t size)
{
    for (std::size_t i = 0; i < size; ++i)
    {
        if (read_byte(dest[i]) != 1)
        {
            return i;
        }
    }
    return size;
}

int vm::Reader::read_byte(byte &dest)
{
    refill_buffer();

    if (_pos >= _limit)
    {
        return 0; // No more data to read
    }

    dest = _buffer[_pos++];
    return 1;
}