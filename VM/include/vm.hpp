#ifndef SHELLVM_VM
#define SHELLVM_VM

#include <fstream>
#include <filesystem>
#include <cstdint>

namespace fs = std::filesystem;

namespace vm {
    
void proccess(const fs::path &);

class Reader {
public:
using byte = std::uint8_t;
using word = std::uint16_t;

    Reader(const fs::path &);
    Reader(const Reader &) = delete;
    Reader(Reader &&);
    Reader &operator=(const Reader &) = delete;
    Reader &operator=(Reader &&);

    bool read_word(word &);
    std::size_t read_bytes(byte *, std::size_t);

    ~Reader();

private:
    constexpr static std::size_t DEFAULT_BUFFER_SIZE = 1024;
    constexpr static std::size_t WORD_SIZE = 2;

    std::ifstream _input;
    byte *_buffer;
    std::size_t _capacity;
    std::size_t _limit;
    std::size_t _pos;

    void refill_buffer();
    int read_byte(byte &);
};

}

#endif