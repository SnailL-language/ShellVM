#ifndef SHELLVM_VM
#define SHELLVM_VM

#include <fstream>
#include <filesystem>
#include <cstdint>

namespace fs = std::filesystem;

namespace vm {
    
void proccess(const fs::path &);

namespace code {

typedef std::uint8_t byte;
typedef std::uint16_t u16;
typedef std::uint32_t u32;

class InvalidBytecodeException
{
public: 
    InvalidBytecodeException(std::string message): message{message}{}
    std::string getMessage() const {return message;}
private:
    std::string message;
};

struct Header {
    uint32_t magic;
    uint16_t version;
    uint16_t main_function_index;
};

struct ConstantPool {
    u16 size;
    byte **data;

    ConstantPool(u16, byte **);
    ConstantPool(const ConstantPool &other) = delete;
    ConstantPool(ConstantPool &&other);

    ConstantPool &operator=(const ConstantPool &other) = delete;
    ConstantPool &operator=(ConstantPool &&other);

    ~ConstantPool();
};

class Reader {
public:
    Reader(const fs::path &);
    Reader(const Reader &) = delete;
    Reader(Reader &&);
    Reader &operator=(const Reader &) = delete;
    Reader &operator=(Reader &&);

    Header read_header();
    ConstantPool read_constants();

    void close();

    ~Reader();

private:
    constexpr static std::size_t DEFAULT_BUFFER_SIZE = 1024;

    std::ifstream _input;
    byte *_buffer;
    std::size_t _capacity;
    std::size_t _limit;
    std::size_t _pos;

    u16 read_16();
    u32 read_32();
    void read_big_endian(byte *, std::size_t);
    void read_bytes(byte *, std::size_t);

    byte read_byte();
    void refill_buffer();
};

}

}

#endif