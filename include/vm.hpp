#ifndef SHELLVM_VM
#define SHELLVM_VM

#include <fstream>
#include <filesystem>
#include <functional>
#include <cstdint>
#include <vector>

namespace fs = std::filesystem;

typedef std::uint8_t byte;
typedef std::uint16_t u16;
typedef std::uint32_t u32;

namespace vm
{
    namespace runtime
    {

        class HaltException
        {
        public:
            HaltException(std::string message) : message{message} {}
            std::string getMessage() const { return message; }

        private:
            std::string message;
        };

        enum Type : byte
        {
            VOID = 0x00,
            I32 = 0x01,
            USIZE = 0x02,
            STRING = 0x03,
            ARRAY = 0x04
        };

        struct Object
        {
            Type type;
            byte *data;
            std::size_t data_size;
            std::size_t links;

            Object(Type, const byte *, std::size_t);
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
            explicit operator u32() const;
            explicit operator std::string() const;

            ~Object();
        };

        struct Link
        {
            Object *object = nullptr;

            Link();
            Link(const Link &) = delete;
            Link(Link &&) = delete;

            Link &operator=(const Link &) = delete;
            Link &operator=(Link &&) = delete;
            Link &operator=(Object *&);

            ~Link();
        };

        struct GlobalVariables
        {
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

    namespace memory
    {

        class Allocator
        {
        public:
            Allocator() = default;
            Allocator(const Allocator &) = delete;
            Allocator(Allocator &&) = default;

            Allocator &operator=(const Allocator &) = delete;
            Allocator &operator=(Allocator &&) = delete;

            runtime::Object *create(runtime::Type, const byte *, std::size_t);

            std::size_t size() const;

            ~Allocator();

        private:
            std::vector<runtime::Object *> allocated_objects;

            void collect_garbage();
        };

    }

    namespace code
    {

        enum Command : byte
        {
            PUSH_CONST = 0x01,
            PUSH_LOCAL = 0x02,
            PUSH_GLOBAL = 0x03,
            STORE_LOCAL = 0x04,
            STORE_GLOBAL = 0x05,
            POP = 0x06,
            DUP = 0x07,

            ADD = 0x10,
            SUB = 0x11,
            MUL = 0x12,
            DIV = 0x13,
            MOD = 0x14,

            EQ = 0x20,
            NEQ = 0x21,
            LT = 0x22,
            LE = 0x23,
            GT = 0x24,
            GTE = 0x25,
            AND = 0x26,
            OR = 0x27,
            NOT = 0x28,

            JMP = 0x30,
            JMP_IF_FALSE = 0x31,
            JMP_IF_TRUE = 0x35,
            CALL = 0x32,
            RET = 0x33,
            HALT = 0x34,

            NEW_ARRAY = 0x40,
            GET_ARRAY = 0x41,
            SET_ARRAY = 0x42,
            INIT_ARRAY = 0x43,

            INTRINSIC_CALL = 0x50
        };

        class InvalidBytecodeException
        {
        public:
            InvalidBytecodeException(std::string message) : message{message} {}
            std::string getMessage() const { return message; }

        private:
            std::string message;
        };

        struct Header
        {
            uint32_t magic;
            uint16_t version;
            uint16_t main_function_index;
        };

        struct ConstantPool
        {
            u16 size;
            runtime::Object **data;

            ConstantPool(u16, runtime::Object **);
            ConstantPool(const ConstantPool &other) = delete;
            ConstantPool(ConstantPool &&other);

            ConstantPool &operator=(const ConstantPool &other) = delete;
            ConstantPool &operator=(ConstantPool &&other);

            ~ConstantPool();
        };

        struct Function
        {
            std::size_t offset;
            runtime::Type return_type;
            byte arg_count;
            u16 local_count;
            u32 length;
            std::size_t calls = 0;
            void *compiled = nullptr;
        };

        struct FunctionTable
        {
            u16 size;
            Function *functions;

            FunctionTable(u16, Function *);
            FunctionTable(const FunctionTable &) = delete;
            FunctionTable(FunctionTable &&other);

            FunctionTable &operator=(const FunctionTable &) = delete;
            FunctionTable &operator=(FunctionTable &&other);

            ~FunctionTable();
        };

        struct Intrinsic
        {
            runtime::Type return_type;
            byte arg_count;
            std::string name;
        };

        struct IntrinsicTable
        {
            u16 size;
            Intrinsic *functions;

            IntrinsicTable(u16, Intrinsic *);
            IntrinsicTable(const IntrinsicTable &) = delete;
            IntrinsicTable(IntrinsicTable &&other);

            IntrinsicTable &operator=(const IntrinsicTable &) = delete;
            IntrinsicTable &operator=(IntrinsicTable &&other);

            ~IntrinsicTable();
        };

        class Reader
        {
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

    struct Environment
    {
        memory::Allocator allocator;
        code::Header header;
        code::ConstantPool constant_pool;
        runtime::GlobalVariables global;
        code::FunctionTable functions;
        code::IntrinsicTable intrinsics;
        std::stack<runtime::Object *> stack;

        Environment(
            memory::Allocator &allocator,
            code::Header &&header,
            code::ConstantPool &&pool,
            runtime::GlobalVariables &&global,
            code::FunctionTable &&functions,
            code::IntrinsicTable &&intrinsics);
    };

    void process(const fs::path &, bool);

    void process(code::Reader &reader, Environment &env, std::size_t, std::size_t, bool);

    namespace jit
    {
        void compile_func(code::Reader &, int, code::Function &, bool);
    }

    namespace proccess
    {

        using jit_function = void(code::Reader &reader,
                                  Environment &env,
                                  std::function<void(runtime::Object *)> push,
                                  std::function<void()> pop,
                                  std::function<void(const char *, std::function<int(int &&, int &&)>, std::function<u32(u32 &&, u32 &&)>, std::function<std::string(std::string &&, std::string &&)>)> arithmetic_operation,
                                  std::function<void(const char *, std::function<bool(int &&, int &&)>, std::function<bool(u32 &&, u32 &&)>)> compare_operation,
                                  std::function<void(const char *, std::function<bool(bool &&, bool &&)>)> logical_operation,
                                  bool debug_mode);

        void call_intrinsic(u16, Environment &, bool);

        template <typename T>
        inline std::function<T(T &&, T &&)> get_arithmetic_function(byte command)
        {
            switch (command)
            {
            case code::Command::ADD:
                return [](T &&a, T &&b)
                { return a + b; };
            case code::Command::SUB:
                return [](T &&a, T &&b)
                { return a - b; };
            case code::Command::MUL:
                return [](T &&a, T &&b)
                { return a * b; };
            case code::Command::DIV:
                return [](T &&a, T &&b)
                { return a / b; };
            case code::Command::MOD:
                return [](T &&a, T &&b)
                { return a % b; };
            default:
                throw code::InvalidBytecodeException("Invalid arithmetic command");
            }
        }

        template <typename T>
        inline std::function<bool(T &&, T &&)> get_comparison_function(byte command)
        {
            switch (command)
            {
            case code::Command::EQ:
                return [](T &&a, T &&b)
                { return a == b; };
            case code::Command::NEQ:
                return [](T &&a, T &&b)
                { return a != b; };
            case code::Command::LT:
                return [](T &&a, T &&b)
                { return a < b; };
            case code::Command::LE:
                return [](T &&a, T &&b)
                { return a <= b; };
            case code::Command::GT:
                return [](T &&a, T &&b)
                { return a > b; };
            case code::Command::GTE:
                return [](T &&a, T &&b)
                { return a >= b; };
            default:
                throw code::InvalidBytecodeException("Invalid comparison command");
            }
        }

        inline std::function<bool(bool &&, bool &&)> get_logical_function(byte command)
        {
            switch (command)
            {
            case code::Command::AND:
                return [](bool &&a, bool &&b)
                { return a && b; };
            case code::Command::OR:
                return [](bool &&a, bool &&b)
                { return a || b; };
            default:
                throw code::InvalidBytecodeException("Invalid logical command");
            }
        }

    }

}

#endif