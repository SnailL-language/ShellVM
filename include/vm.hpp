#ifndef SHELLVM_VM
#define SHELLVM_VM

#include <fstream>
#include <filesystem>
#include <cstdint>
#include <vector>

namespace fs = std::filesystem;

typedef std::uint8_t byte;
typedef std::uint16_t u16;
typedef std::uint32_t u32;

namespace vm {
    
void process(const fs::path &);

namespace runtime {

class HaltException
{
public: 
    HaltException(std::string message): message{message}{}
    std::string getMessage() const {return message;}
private:
    std::string message;
};

enum class Type {
    VOID = 0x00, 
    I32 = 0x01, 
    USIZE = 0x02, 
    STRING = 0x03, 
    ARRAY = 0x04 
};

struct Object {
    Type type;
    byte *data;
    std::size_t data_size;
    std::size_t links;

    Object(Type, byte *, std::size_t);
    Object(const Object &) = delete;
    Object(Object &&) = delete;

    Object &operator=(const Object &) = delete;
    Object &operator=(Object &&) = delete;

    bool operator==(const Object &) const;
    bool operator!=(const Object &) const;

    bool operator<=(const Object &) const;
    bool operator>=(const Object &) const;
    bool operator<(const Object &) const;
    bool operator>(const Object &) const;

    explicit operator bool() const;
    explicit operator int() const;
    explicit operator std::size_t() const;
    explicit operator std::string() const;

    ~Object();
};

struct Link {
    Object *object = nullptr;

    Link &operator=(Object *&);

    ~Link();
};

struct GlobalVariables {
    u16 size;
    Link *variables;

    GlobalVariables(u16, Link *);
    GlobalVariables(const GlobalVariables &) = delete;
    GlobalVariables(GlobalVariables &&);

    GlobalVariables &operator=(const GlobalVariables &) = delete;
    GlobalVariables &operator=(GlobalVariables &&);

    ~GlobalVariables();
};

}

namespace memory {

class Allocator 
{
public: 
    Allocator() = default;
    Allocator(const Allocator &) = delete;
    Allocator(Allocator &&) = delete;

    Allocator &operator=(const Allocator &) = delete;
    Allocator &operator=(Allocator &&) = delete;

    runtime::Object *create(runtime::Type, byte *, std::size_t);

    std::size_t size() const;

    ~Allocator();

private:
    std::vector<runtime::Object *> allocated_objects;

    void collect_garbage();
};

}

namespace code {

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
    runtime::Object **data;

    ConstantPool(u16, runtime::Object **);
    ConstantPool(const ConstantPool &other) = delete;
    ConstantPool(ConstantPool &&other);

    ConstantPool &operator=(const ConstantPool &other) = delete;
    ConstantPool &operator=(ConstantPool &&other);

    ~ConstantPool();
};

struct Function {
    std::size_t offset;
    runtime::Type return_type;
    byte arg_count;
    u16 local_count;
    u32 length;
};

struct FunctionTable {
    u16 size;
    Function *functions;

    FunctionTable(u16, Function *);
    FunctionTable(const FunctionTable &) = delete;
    FunctionTable(FunctionTable &&other);

    FunctionTable &operator=(const FunctionTable &) = delete;
    FunctionTable &operator=(FunctionTable &&other);

    ~FunctionTable();
};

struct Intrinsic {
    runtime::Type return_type;
    byte arg_count;
    std::string name;
};

struct IntrinsicTable {
    u16 size;
    Intrinsic *functions;

    IntrinsicTable(u16, Intrinsic *);
    IntrinsicTable(const IntrinsicTable &) = delete;
    IntrinsicTable(IntrinsicTable &&other);

    IntrinsicTable &operator=(const IntrinsicTable &) = delete;
    IntrinsicTable &operator=(IntrinsicTable &&other);

    ~IntrinsicTable();
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
    runtime::GlobalVariables read_globals();
    FunctionTable read_functions();
    IntrinsicTable read_intrinsics();

    runtime::Type read_type();
    byte read_byte();
    u16 read_16();
    u32 read_32();

    std::size_t get_offset() const;
    void set_offset(std::size_t);

    void skip(std::size_t);

    void close();

    ~Reader();

private:
    constexpr static std::size_t DEFAULT_BUFFER_SIZE = 1024;

    std::ifstream _input;
    byte *_buffer;
    std::size_t _capacity;
    std::size_t _limit;
    std::size_t _pos;
    std::size_t _absolute_pos;

    
    void read_big_endian(byte *, std::size_t);
    void read_bytes(byte *, std::size_t);

    void refill_buffer();
};

}

}

#endif