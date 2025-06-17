#include "vm.hpp"
#include <vector>
#include <stack>
#include <string>
#include <iostream>
#include <typeinfo>

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
    return header;
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
        code::IntrinsicTable &&intrinsics)
        : allocator(std::move(allocator)), header(std::move(header)), constant_pool(std::move(pool)), global(std::move(global)), functions(std::move(functions)), intrinsics(std::move(intrinsics))
    {
    }
};

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
class Intrinsic
{
public:
    constexpr static std::string PRINTLN = "println";
};

static void pop(std::stack<runtime::Object *> &stack) {
    stack.top()->links--;
    stack.pop();
}

static void push(std::stack<runtime::Object *> &stack, runtime::Object *obj) {
    obj->links++;
    stack.push(obj);
}

static void call_intrinsic(u16 index, Environment &env, bool debug_mode)
{
    if (env.intrinsics.functions[index].name == Intrinsic::PRINTLN)
    {
        if (debug_mode) std::cout << "=====================================" << std::endl;
        if (debug_mode) std::cout << "Output:" << std::endl;
        std::cout << static_cast<std::string>(*env.stack.top()) << std::endl;
        if (debug_mode) std::cout << "=====================================" << std::endl;
        pop(env.stack);
    }
    else 
    {
        throw code::InvalidBytecodeException("Unsupported intrinsic function");
    }
}

static void process(code::Reader &reader, Environment &env, std::size_t length, std::size_t local_count, bool debug_mode)
{
    std::size_t start = reader.get_offset();
    std::vector<runtime::Link> local_variables(local_count);
    while (reader.get_offset() - start < length)
    {
        byte command = reader.read_byte();
        switch (command)
        {
        case Command::PUSH_CONST:
        {
            u16 index = reader.read_16();
            //DEBUG
            if (debug_mode) std::cout << "PUSH_CONST from index " << index << " " << static_cast<std::string>(*env.constant_pool.data[index]) << std::endl;
            push(env.stack, env.constant_pool.data[index]);
            break;
        }
        case Command::PUSH_LOCAL:
        {
            u16 index = reader.read_16();
            //DEBUG
            if (debug_mode) std::cout << "PUSH_LOCAL from index " << index << std::endl;
            push(env.stack, local_variables[index].object);
            break;
        }
        case Command::PUSH_GLOBAL:
        {
            u16 index = reader.read_16();
            //DEBUG
            if (debug_mode) std::cout << "PUSH_GLOBAL from index " << index << std::endl;
            push(env.stack, env.global.variables[index].object);
            break;
        }
        case Command::STORE_LOCAL:
        {
            u16 index = reader.read_16();
            //DEBUG
            if (debug_mode) std::cout << "STORE_LOCAL to index " << index << std::endl;
            local_variables[index] = env.stack.top();
            pop(env.stack);
            break;
        }
        case Command::STORE_GLOBAL:
        {
            u16 index = reader.read_16();
            //DEBUG
            if (debug_mode) std::cout << "STORE_GLOBAL to index " << index << std::endl;
            env.global.variables[index] = env.stack.top();
            pop(env.stack);
            break;
        }
        case Command::POP:
        {
            //DEBUG
            if (debug_mode) std::cout << "POP " << std::endl;
            pop(env.stack);
            break;
        }
        case Command::DUP:
        {
            //DEBUG
            if (debug_mode) std::cout << "DUP " << std::endl;
            push(env.stack, env.stack.top());
            break;
        }

        case Command::ADD:
        {
            runtime::Object *right = env.stack.top();
            pop(env.stack);
            runtime::Object *left = env.stack.top();
            pop(env.stack);
            //DEBUG
            if (debug_mode) std::cout << "ADD of " << static_cast<std::string>(*left) << " " << static_cast<std::string>(*right) << std::endl;
            runtime::Object *obj;
            switch (left->type)
            {
            case runtime::Type::I32:
                obj = env.allocator.create(runtime::Type::I32, reinterpret_cast<byte *>(new int(static_cast<int>(*left) + static_cast<int>(*right))), 4);
                break;
            case runtime::Type::USIZE:
                obj = env.allocator.create(runtime::Type::USIZE, reinterpret_cast<byte *>(new std::size_t(static_cast<u32>(*left) + static_cast<u32>(*right))), 4);
                break;
            default:
                std::string sum = static_cast<std::string>(*left) + static_cast<std::string>(*right);
                std::size_t len = sum.size(); 
                byte *data = new byte[len];
                std::copy(sum.c_str(), sum.c_str() + len, data);
                obj = env.allocator.create(runtime::Type::STRING, data, len);
                break;
            }
            push(env.stack, obj);
            break;
        }
        case Command::SUB:
        {
            runtime::Object *right = env.stack.top();
            pop(env.stack);
            runtime::Object *left = env.stack.top();
            pop(env.stack);
            //DEBUG
            if (debug_mode) std::cout << "SUB of " << static_cast<std::string>(*left) << " " << static_cast<std::string>(*right) << std::endl;
            runtime::Object *obj;
            switch (left->type)
            {
            case runtime::Type::I32:
                obj = env.allocator.create(runtime::Type::I32, reinterpret_cast<byte *>(new int(static_cast<int>(*left) - static_cast<int>(*right))), 4);
                break;
            case runtime::Type::USIZE:
                obj = env.allocator.create(runtime::Type::USIZE, reinterpret_cast<byte *>(new std::size_t(static_cast<u32>(*left) - static_cast<u32>(*right))), sizeof(std::size_t));
                break;
            default:
                throw code::InvalidBytecodeException("Invalid type for SUB");
            }
            push(env.stack, obj);
            break;
        }
        case Command::MUL:
        {
            runtime::Object *right = env.stack.top();
            pop(env.stack);
            runtime::Object *left = env.stack.top();
            pop(env.stack);
            //DEBUG
            if (debug_mode) std::cout << "MUL of " << static_cast<std::string>(*left) << " " << static_cast<std::string>(*right) << std::endl;
            runtime::Object *obj;
            switch (left->type)
            {
            case runtime::Type::I32:
                obj = env.allocator.create(runtime::Type::I32, reinterpret_cast<byte *>(new int(static_cast<int>(*left) * static_cast<int>(*right))), 4);
                break;
            case runtime::Type::USIZE:
                obj = env.allocator.create(runtime::Type::USIZE, reinterpret_cast<byte *>(new std::size_t(static_cast<u32>(*left) * static_cast<u32>(*right))), sizeof(std::size_t));
                break;
            default:
                throw code::InvalidBytecodeException("Invalid type for MUL");
            }
            push(env.stack, obj);
            break;
        }
        case Command::DIV:
        {
            runtime::Object *right = env.stack.top();
            pop(env.stack);
            runtime::Object *left = env.stack.top();
            pop(env.stack);
            //DEBUG
            if (debug_mode) std::cout << "DIV of " << static_cast<std::string>(*left) << " " << static_cast<std::string>(*right) << std::endl;
            runtime::Object *obj;
            switch (left->type)
            {
            case runtime::Type::I32:
                obj = env.allocator.create(runtime::Type::I32, reinterpret_cast<byte *>(new int(static_cast<int>(*left) / static_cast<int>(*right))), 4);
                break;
            case runtime::Type::USIZE:
                obj = env.allocator.create(runtime::Type::USIZE, reinterpret_cast<byte *>(new std::size_t(static_cast<u32>(*left) / static_cast<u32>(*right))), sizeof(std::size_t));
                break;
            default:
                throw code::InvalidBytecodeException("Invalid type for DIV");
            }
            push(env.stack, obj);
            break;
        }
        case Command::MOD:
        {
            runtime::Object *right = env.stack.top();
            pop(env.stack);
            runtime::Object *left = env.stack.top();
            pop(env.stack);
            //DEBUG
            if (debug_mode) std::cout << "MOD of " << (int)*left << " " << (int)*right << std::endl;
            runtime::Object *obj;
            switch (left->type)
            {
            case runtime::Type::I32:
                obj = env.allocator.create(runtime::Type::I32, reinterpret_cast<byte *>(new int(static_cast<int>(*left) % static_cast<int>(*right))), 4);
                break;
            case runtime::Type::USIZE:
                obj = env.allocator.create(runtime::Type::USIZE, reinterpret_cast<byte *>(new std::size_t(static_cast<u32>(*left) % static_cast<u32>(*right))), sizeof(std::size_t));
                break;
            default:
                throw code::InvalidBytecodeException("Invalid type for MOD");
            }
            push(env.stack, obj);
            break;
        }

        case Command::EQ:
        {
            runtime::Object *right = env.stack.top();
            pop(env.stack);
            runtime::Object *left = env.stack.top();
            pop(env.stack);
            //DEBUG
            if (debug_mode) std::cout << "EQ of " << (int)*left << " " << (int)*right << std::endl;
            push(env.stack, env.allocator.create(runtime::Type::I32, reinterpret_cast<byte *>(new int(
                *left == *right
            )), 4));
            break;
        }
        case Command::NEQ:
        {
            runtime::Object *right = env.stack.top();
            pop(env.stack);
            runtime::Object *left = env.stack.top();
            pop(env.stack);
            //DEBUG
            if (debug_mode) std::cout << "NEQ of " << (int)*left << " " << (int)*right << std::endl;
            push(env.stack, env.allocator.create(runtime::Type::I32, reinterpret_cast<byte *>(new int(
                *left != *right
            )), 4));
            break;
        }
        case Command::LT:
        {
            runtime::Object *right = env.stack.top();
            pop(env.stack);
            runtime::Object *left = env.stack.top();
            pop(env.stack);
            //DEBUG
            if (debug_mode) std::cout << "LT of " << (int)*left << " " << (int)*right << std::endl;
            push(env.stack, env.allocator.create(runtime::Type::I32, reinterpret_cast<byte *>(new int(
                *left < *right
            )), 4));
            break;
        }
        case Command::LE:
        {
            runtime::Object *right = env.stack.top();
            pop(env.stack);
            runtime::Object *left = env.stack.top();
            pop(env.stack);
            //DEBUG
            if (debug_mode) std::cout << "LE of " << (int)*left << " " << (int)*right << std::endl;
            push(env.stack, env.allocator.create(runtime::Type::I32, reinterpret_cast<byte *>(new int(
                *left <= *right
            )), 4));
            break;
        }
        case Command::GT:
        {
            runtime::Object *right = env.stack.top();
            pop(env.stack);
            runtime::Object *left = env.stack.top();
            pop(env.stack);
            //DEBUG
            if (debug_mode) std::cout << "GT of " << (int)*left << " " << (int)*right << std::endl;
            push(env.stack, env.allocator.create(runtime::Type::I32, reinterpret_cast<byte *>(new int(
                *left > *right
            )), 4));
            break;
        }
        case Command::GTE:
        {
            runtime::Object *right = env.stack.top();
            pop(env.stack);
            runtime::Object *left = env.stack.top();
            pop(env.stack);
            //DEBUG
            if (debug_mode) std::cout << "GTE of " << (int)*left << " " << (int)*right << std::endl;
            push(env.stack, env.allocator.create(runtime::Type::I32, reinterpret_cast<byte *>(new int(
                *left >= *right
            )), 4));
            break;
        }
        case Command::AND:
        {
            runtime::Object *right = env.stack.top();
            pop(env.stack);
            runtime::Object *left = env.stack.top();
            pop(env.stack);
            //DEBUG
            if (debug_mode) std::cout << "AND of " << (int)*left << " " << (int)*right << std::endl;
            push(env.stack, env.allocator.create(runtime::Type::I32, reinterpret_cast<byte *>(new int(
                static_cast<bool>(*left) && static_cast<bool>(*right)
            )), 4));
            break;
        }
        case Command::OR:
        {
            runtime::Object *right = env.stack.top();
            pop(env.stack);
            runtime::Object *left = env.stack.top();
            pop(env.stack);
            //DEBUG
            if (debug_mode) std::cout << "OR of " << (int)*left << " " << (int)*right << std::endl;
            push(env.stack, env.allocator.create(runtime::Type::I32, reinterpret_cast<byte *>(new int(
                static_cast<bool>(*left) || static_cast<bool>(*right)
            )), 4));
            break;
        }
        case Command::NOT:
        {
            runtime::Object *obj = env.stack.top();
            pop(env.stack);
            //DEBUG
            if (debug_mode) std::cout << "Not of " << (int)*obj << std::endl;
            push(env.stack, env.allocator.create(runtime::Type::I32, reinterpret_cast<byte *>(new int(
                !static_cast<bool>(*obj)
            )), 4));
            break;
        }

        case Command::JMP:
        {
            int length = static_cast<std::int16_t>(reader.read_16());
            //DEBUG
            if (debug_mode) std::cout << "JMP to " << length << std::endl;
            if (length < 0) {
                reader.set_offset(reader.get_offset() + length);
            } else {
                reader.skip(length);
            }
            break;
        }
        case Command::JMP_IF_FALSE:
        {
            int length = static_cast<std::int16_t>(reader.read_16());
            runtime::Object *condition = env.stack.top();
            pop(env.stack);
            //DEBUG
            if (debug_mode) std::cout << "JMP_IF_FALSE " << length << " " << (bool)*condition << std::endl;
            if (!static_cast<bool>(*condition))
            {
                if (length < 0) {
                    reader.set_offset(reader.get_offset() + length);
                } else {
                    reader.skip(length);
                }
            }
            break;
        }
        case Command::JMP_IF_TRUE:
        {
            int length = static_cast<std::int16_t>(reader.read_16());
            runtime::Object *condition = env.stack.top();
            pop(env.stack);
            //DEBUG
            if (debug_mode) std::cout << "JMP_IF_TRUE " << length << " " << (bool)*condition << std::endl;
            if (static_cast<bool>(*condition))
            {
                if (length < 0) {
                    reader.set_offset(reader.get_offset() + length);
                } else {
                    reader.skip(length);
                }
            }
            break;
        }
        case Command::CALL:
        {
            u16 index = reader.read_16();
            std::size_t current_addr = reader.get_offset();
            code::Function func = env.functions.functions[index];
            //DEBUG
            if (debug_mode) std::cout << "CALL of " << index << " on offset " << reader.get_offset() << " to offset " << func.offset << std::endl;
            reader.set_offset(func.offset);
            if (debug_mode) std::cout << "new_offset " << reader.get_offset() << std::endl;
            process(reader, env, func.length, func.local_count + func.arg_count, debug_mode);
            reader.set_offset(current_addr);
            break;
        }
        case Command::RET:
        {
            //DEBUG
            if (debug_mode) std::cout << "RET " << std::endl;
            return;
        }
        case Command::HALT:
        {
            //DEBUG
            if (debug_mode) std::cout << "HALT " << std::endl;
            throw runtime::HaltException("HALT command found in bytecode!");
        }

        case Command::NEW_ARRAY:
        {
            u32 size = reader.read_32();
            //DEBUG
            if (debug_mode) std::cout << "NEW_ARRAY of " << size << " elements" << std::endl;
            // runtime::Type type = reader.read_type();
            reader.skip(1U);
            push(env.stack, env.allocator.create(runtime::Type::ARRAY, nullptr, size));
            break;
        }
        case Command::GET_ARRAY:
        {
            runtime::Object *index = env.stack.top();
            pop(env.stack);
            runtime::Object *array = env.stack.top();
            pop(env.stack);
            //DEBUG
            if (debug_mode) std::cout << "GET_ARRAY in " << (int)*index << std::endl;
            push(env.stack, reinterpret_cast<runtime::Link *>(array->data)[static_cast<u32>(*index)].object);
            break;
        }
        case Command::SET_ARRAY:
        {
            runtime::Object *index = env.stack.top();
            pop(env.stack);
            runtime::Object *value = env.stack.top();
            pop(env.stack);
            runtime::Object *array = env.stack.top();
            pop(env.stack);
            //DEBUG
            if (debug_mode) std::cout << "SET_ARRAY in " << static_cast<u32>(*index) << " with " << static_cast<int>(*value) << std::endl;
            reinterpret_cast<runtime::Link *>(array->data)[static_cast<u32>(*index)] = value;
            break;
        }
        case Command::INIT_ARRAY:
        {
            u16 size = reader.read_16();
            //DEBUG
            if (debug_mode) std::cout << "INIT_ARRAY of size " << size << std::endl;
            runtime::Object **objects = new runtime::Object *[size];
            for (u16 i = 0; i < size; ++i) {
                objects[i] = env.stack.top();
                pop(env.stack);
            }
            runtime::Object *array = env.stack.top();
            pop(env.stack);
            for (u16 i = 0; i < size; ++i) {
                reinterpret_cast<runtime::Link *>(array->data)[i] = objects[i];
            }
            delete[] objects;
            push(env.stack, array);
            break;
        }
        case Command::INTRINSIC_CALL:
        {
            u16 index = reader.read_16();
            //DEBUG
            if (debug_mode) std::cout << "INTRINSIC_CALL of " << index << std::endl;
            call_intrinsic(index, env, debug_mode);
            break;
        }
        }
        //DEBUG
        if (debug_mode) std::cout << "Stack size: " << env.stack.size() << std::endl;
    }
}

void vm::process(const fs::path &file, bool debug_mode)
{
    code::Reader reader(file);
    memory::Allocator allocator;
    Environment env(allocator, parse_header(reader), reader.read_constants(), reader.read_globals(allocator), reader.read_functions(), reader.read_intrinsics());
    u32 length = reader.read_32();
    process(reader, env, length, 0, debug_mode);
}