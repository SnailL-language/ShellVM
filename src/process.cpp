#include "vm.hpp"
#include <vector>
#include <stack>

using namespace vm;

static code::Header parse_header(code::Reader &reader)
{
    code::Header header = reader.read_header();
    if (header.magic != 0x534E4131U)
    {
        throw code::InvalidBytecodeException("Magic constant is invalid!");
    }
    if (header.version)
    {
    }
    if (header.main_function_index < 0)
    {
        throw code::InvalidBytecodeException("Cannot run bytecode without a main function");
    }
}

struct Environment
{
    code::Header header;
    code::ConstantPool constant_pool;
    runtime::GlobalVariables global;
    code::FunctionTable functions;
    code::IntrinsicTable intrinsics;
    memory::Allocator allocator;
    std::stack<runtime::Object *> stack;

    Environment(
        code::Header &&header,
        code::ConstantPool &&pool,
        runtime::GlobalVariables &&global,
        code::FunctionTable &&functions,
        code::IntrinsicTable &&intrinsics)
        : header(std::move(header)), constant_pool(std::move(pool)), global(std::move(global)), functions(std::move(functions)), intrinsics(std::move(intrinsics))
    {
    }
};

enum Command : byte {
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
    SET_ARRAY = 0x42
};

static void process(code::Reader &reader, Environment &env, std::size_t length, std::size_t local_count) {
    std::size_t start = reader.get_offset();
    std::vector<runtime::Link> local_variables(local_count);
    while (reader.get_offset() - start < length) {
        byte command = reader.read_byte();
        switch (command) {
        case Command::PUSH_CONST:
        {
            u16 index = reader.read_16();
            env.stack.push(env.constant_pool.data[index]);
            break;
        }
        case Command::PUSH_LOCAL: 
        {
            u16 index = reader.read_16();
            env.stack.push(local_variables[index].object); 
            break; 
        }
        case Command::PUSH_GLOBAL: 
        {
            u16 index = reader.read_16();
            env.stack.push(env.global.variables[index].object); 
            break; 
        }
        case Command::STORE_LOCAL: 
        {
            u16 index = reader.read_16();
            local_variables[index] = env.stack.top();
            env.stack.pop(); 
            break; 
        }
        case Command::STORE_GLOBAL: 
        {
            u16 index = reader.read_16();
            env.global.variables[index] = env.stack.top();
            env.stack.pop(); 
            break; 
        }
        case Command::POP: 
        {
            env.stack.pop(); 
            break; 
        }
        case Command::DUP: 
        {
            env.stack.push(env.stack.top()); 
            break; 
        }

        case Command::ADD: { break; }
        case Command::SUB: { break; }
        case Command::MUL: { break; }
        case Command::DIV: { break; }
        case Command::MOD: { break; }

        case Command::EQ: { break; }
        case Command::NEQ: { break; }
        case Command::LT: { break; }
        case Command::LE: { break; }
        case Command::GT: { break; }
        case Command::GTE: { break; }
        case Command::AND: { break; }
        case Command::OR: { break; }
        case Command::NOT: { break; }

        case Command::JMP: 
        {
            u16 length = reader.read_16();
            reader.skip(length);
            break; 
        }
        case Command::JMP_IF_FALSE: 
        { 
            u16 length = reader.read_16();
            runtime::Object *condition = env.stack.top();
            env.stack.pop();
            if (!static_cast<bool>(condition)) {
                reader.skip(length);
            }
            break; 
        }
        case Command::JMP_IF_TRUE: 
        { 
            u16 length = reader.read_16();
            runtime::Object *condition = env.stack.top();
            env.stack.pop();
            if (static_cast<bool>(*condition)) {
                reader.skip(length);
            }
            break; 
        }
        case Command::CALL: 
        { 
            u16 index = reader.read_16();
            std::size_t current_addr = reader.get_offset();
            code::Function func = env.functions.functions[index];
            reader.set_offset(func.offset);
            process(reader, env, func.length, func.local_count + func.arg_count);
            break; 
        }
        case Command::RET: { break; }
        case Command::HALT: 
        { 
            throw runtime::HaltException("HALT command found in bytecode!");
        }

        case Command::NEW_ARRAY: 
        { 
            u16 size = reader.read_16();
            runtime::Type type = reader.read_type();
            env.stack.push(env.allocator.create(type, reinterpret_cast<byte *>(new runtime::Link[size]), sizeof(runtime::Link) * size));
            break; 
        }
        case Command::GET_ARRAY: 
        { 
            runtime::Object *array = env.stack.top();
            env.stack.pop();
            runtime::Object *index = env.stack.top();
            env.stack.pop();
            env.stack.push(reinterpret_cast<runtime::Link *>(array->data)[static_cast<std::size_t>(*index)].object);
            break; 
        }
        case Command::SET_ARRAY: 
        { 
            runtime::Object *array = env.stack.top();
            env.stack.pop();
            runtime::Object *index = env.stack.top();
            env.stack.pop();
            runtime::Object *value = env.stack.top();
            env.stack.pop();
            reinterpret_cast<runtime::Link *>(array->data)[static_cast<std::size_t>(*index)] = value;
            break; 
        }
        }
    }
}

void vm::process(const fs::path &file)
{
    code::Reader reader(file);
    Environment env(parse_header(reader), reader.read_constants(), reader.read_globals(), reader.read_functions(), reader.read_intrinsics());
    u32 length = reader.read_32();
}