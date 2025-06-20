#include "vm.hpp"
#include <vector>
#include <stack>
#include <string>
#include <iostream>
#include <functional>

using namespace vm;
using Command = vm::code::Command;

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

class Intrinsic
{
public:
    constexpr static const char *PRINTLN = "println";
};

void proccess::call_intrinsic(u16 index, Environment &env, bool debug_mode)
{
    if (env.intrinsics.functions[index].name == Intrinsic::PRINTLN)
    {
        if (debug_mode)
            std::cout << "=====================================" << std::endl;
        if (debug_mode)
            std::cout << "Output:" << std::endl;
        std::cout << static_cast<std::string>(*env.stack.top()) << std::endl;
        if (debug_mode)
            std::cout << "=====================================" << std::endl;
        env.stack.top()->links--;
        env.stack.pop();
    }
    else
    {
        throw code::InvalidBytecodeException("Unsupported intrinsic function");
    }
}

void vm::process(code::Reader &reader, Environment &env, std::size_t length, std::size_t local_count, bool debug_mode)
{
    auto push = [&env](runtime::Object *obj)
    {
        obj->links++;
        env.stack.push(obj);
    };
    auto pop = [&env]()
    {
        env.stack.top()->links--;
        env.stack.pop();
    };
    auto push_indexed = [&reader, &push, debug_mode](const char *suffix, std::function<runtime::Object *(u16)> getter)
    {
        u16 index = reader.read_16();
        if (debug_mode)
            std::cout << "PUSH_" << suffix << " from index " << index << std::endl;
        push(getter(index));
    };
    auto store_indexed = [&reader, &env, &pop, debug_mode](const char *suffix, std::function<runtime::Link *(u16)> getter)
    {
        u16 index = reader.read_16();
        if (debug_mode)
            std::cout << "STORE_" << suffix << " to index " << index << std::endl;
        *getter(index) = env.stack.top();
        pop();
    };
    auto arithmetic_operation = [&env, &pop, &push, debug_mode](const char *operation, std::function<int(int &&, int &&)> int_func, std::function<u32(u32 &&, u32 &&)> u32_func, std::function<std::string(std::string &&, std::string &&)> str_func)
    {
        runtime::Object *right = env.stack.top();
        pop();
        runtime::Object *left = env.stack.top();
        pop();
        if (debug_mode)
            std::cout << operation << " of " << static_cast<std::string>(*left) << " " << static_cast<std::string>(*right) << std::endl;
        runtime::Object *obj;
        switch (std::max(left->type, right->type))
        {
        case runtime::Type::I32:
        {
            int result = int_func(static_cast<int>(*left), static_cast<int>(*right));
            obj = env.allocator.create(runtime::Type::I32, reinterpret_cast<byte *>(&result), sizeof(int));
            break;
        }
        case runtime::Type::USIZE:
        {
            u32 result = u32_func(static_cast<u32>(*left), static_cast<u32>(*right));
            obj = env.allocator.create(runtime::Type::USIZE, reinterpret_cast<byte *>(&result), sizeof(u32));
            break;
        }
        case runtime::Type::STRING:
        {
            std::string result = str_func(static_cast<std::string>(*left), static_cast<std::string>(*right));
            obj = env.allocator.create(runtime::Type::STRING, reinterpret_cast<const byte *>(result.c_str()), result.size());
            break;
        }
        default:
            throw code::InvalidBytecodeException("Invalid type for " + std::string(operation));
        }
        push(obj);
    };
    auto compare_operation = [&env, &pop, &push, debug_mode](const char *operation, std::function<bool(int &&, int &&)> int_func, std::function<bool(u32 &&, u32 &&)> u32_func)
    {
        runtime::Object *right = env.stack.top();
        pop();
        runtime::Object *left = env.stack.top();
        pop();
        if (debug_mode)
            std::cout << operation << " of " << static_cast<std::string>(*left) << " " << static_cast<std::string>(*right) << std::endl;
        int result = 0;
        switch (std::max(left->type, right->type))
        {
        case runtime::Type::I32:
            result = int_func(static_cast<int>(*left), static_cast<int>(*right)) ? 1 : 0;
            break;
        case runtime::Type::USIZE:
            result = u32_func(static_cast<u32>(*left), static_cast<u32>(*right)) ? 1 : 0;
            break;
        default:
            throw code::InvalidBytecodeException("Invalid type for " + std::string(operation));
        }
        push(env.allocator.create(runtime::Type::I32, reinterpret_cast<byte *>(&result), sizeof(int)));
    };
    auto logical_operation = [&env, &pop, &push, debug_mode](const char *operation, std::function<bool(bool &&, bool &&)> func)
    {
        runtime::Object *right = env.stack.top();
        pop();
        runtime::Object *left = env.stack.top();
        pop();
        if (debug_mode)
            std::cout << operation << " of " << static_cast<std::string>(*left) << " " << static_cast<std::string>(*right) << std::endl;
        int result = func(static_cast<bool>(*left), static_cast<bool>(*right)) ? 1 : 0;
        push(env.allocator.create(runtime::Type::I32, reinterpret_cast<byte *>(&result), 4));
    };
    auto jump_if = [&reader, &env, &pop, debug_mode](bool condition)
    {
        int length = static_cast<std::int16_t>(reader.read_16());
        runtime::Object *condition_obj = env.stack.top();
        pop();
        if (debug_mode)
            std::cout << "JUMP_IF_" << (condition ? "TRUE" : "FALSE") << " to " << length << std::endl;
        if (condition == static_cast<bool>(*condition_obj))
            reader.set_offset(reader.get_offset() + length);
    };

    std::size_t start = reader.get_offset();
    std::vector<runtime::Link> local_variables(local_count);
    while (reader.get_offset() - start < length)
    {
        byte command = reader.read_byte();
        switch (command)
        {
        case Command::PUSH_CONST:
        {
            push_indexed("CONST", [&env](u16 index)
                         { return env.constant_pool.data[index]; });
            break;
        }
        case Command::PUSH_LOCAL:
        {
            push_indexed("LOCAL", [&local_variables](u16 index)
                         { return local_variables[index].object; });
            break;
        }
        case Command::PUSH_GLOBAL:
        {
            push_indexed("GLOBAL", [&env](u16 index)
                         { return env.global.variables[index].object; });
            break;
        }
        case Command::STORE_LOCAL:
        {
            store_indexed("LOCAL", [&local_variables](u16 index)
                          { return &local_variables[index]; });
            break;
        }
        case Command::STORE_GLOBAL:
        {
            store_indexed("GLOBAL", [&env](u16 index)
                          { return &env.global.variables[index]; });
            break;
        }
        case Command::POP:
        {
            if (debug_mode)
                std::cout << "POP " << std::endl;
            pop();
            break;
        }
        case Command::DUP:
        {
            if (debug_mode)
                std::cout << "DUP " << std::endl;
            push(env.stack.top());
            break;
        }

        case Command::ADD:
        case Command::SUB:
        case Command::MUL:
        case Command::DIV:
        case Command::MOD:
        {
            arithmetic_operation(
                command == Command::ADD ? "ADD" : command == Command::SUB ? "SUB"
                                              : command == Command::MUL   ? "MUL"
                                              : command == Command::DIV   ? "DIV"
                                                                          : "MOD",
                proccess::get_arithmetic_function<int>(command),
                proccess::get_arithmetic_function<u32>(command),
                [](std::string &&a, std::string &&b)
                { return a + b; });
            break;
        }

        case Command::EQ:
        case Command::NEQ:
        case Command::LT:
        case Command::LE:
        case Command::GT:
        case Command::GTE:
        {
            compare_operation(
                command == Command::EQ ? "EQ" : command == Command::NEQ ? "NEQ"
                                            : command == Command::LT    ? "LT"
                                            : command == Command::LE    ? "LE"
                                            : command == Command::GT    ? "GT"
                                                                        : "GTE",
                proccess::get_comparison_function<int>(command),
                proccess::get_comparison_function<u32>(command));
            break;
        }
        case Command::AND:
        case Command::OR:
        {
            logical_operation(
                command == Command::AND ? "AND"
                                        : "OR",
                proccess::get_logical_function(command));
            break;
        }
        case Command::NOT:
        {
            runtime::Object *obj = env.stack.top();
            pop();
            if (debug_mode)
                std::cout << "Not of " << (int)*obj << std::endl;
            int result = !static_cast<bool>(*obj);
            push(env.allocator.create(runtime::Type::I32, reinterpret_cast<byte *>(&result), 4));
            break;
        }

        case Command::JMP:
        {
            int length = static_cast<std::int16_t>(reader.read_16());
            if (debug_mode)
                std::cout << "JMP to " << length << std::endl;
            reader.set_offset(reader.get_offset() + length);
            break;
        }
        case Command::JMP_IF_FALSE:
        case Command::JMP_IF_TRUE:
        {
            jump_if(command == Command::JMP_IF_TRUE);
            break;
        }
        case Command::CALL:
        {
            u16 index = reader.read_16();
            code::Function &func = env.functions.functions[index];
            if (debug_mode)
                std::cout << "CALL of " << index << std::endl;
            std::size_t current_addr = reader.get_offset();
            if (func.calls++ > 100)
            {
                if (func.compiled == nullptr)
                {
                    reader.set_offset(func.offset);
                    jit::compile_func(reader, index, func, debug_mode);
                }

                reinterpret_cast<proccess::jit_function *>(func.compiled)(reader, env, push, pop, arithmetic_operation, compare_operation, logical_operation, debug_mode);
            }
            else
            {
                reader.set_offset(func.offset);
                process(reader, env, func.length, func.local_count + func.arg_count, debug_mode);
            }
            reader.set_offset(current_addr);
            break;
        }
        case Command::RET:
        {
            if (debug_mode)
                std::cout << "RET" << std::endl;
            return;
        }
        case Command::HALT:
        {
            if (debug_mode)
                std::cout << "HALT " << std::endl;
            throw runtime::HaltException("HALT command found in bytecode!");
        }

        case Command::NEW_ARRAY:
        {
            u32 size = reader.read_32();
            if (debug_mode)
                std::cout << "NEW_ARRAY of " << size << " elements" << std::endl;
            reader.skip(1U);
            push(env.allocator.create(runtime::Type::ARRAY, nullptr, size));
            break;
        }
        case Command::GET_ARRAY:
        {
            runtime::Object *index = env.stack.top();
            pop();
            runtime::Object *array = env.stack.top();
            pop();
            if (debug_mode)
                std::cout << "GET_ARRAY in " << (int)*index << std::endl;
            push(reinterpret_cast<runtime::Link *>(array->data)[static_cast<u32>(*index)].object);
            break;
        }
        case Command::SET_ARRAY:
        {
            runtime::Object *index = env.stack.top();
            pop();
            runtime::Object *value = env.stack.top();
            pop();
            runtime::Object *array = env.stack.top();
            pop();
            if (debug_mode)
                std::cout << "SET_ARRAY in " << static_cast<u32>(*index) << " with " << static_cast<int>(*value) << std::endl;
            reinterpret_cast<runtime::Link *>(array->data)[static_cast<u32>(*index)] = value;
            break;
        }
        case Command::INIT_ARRAY:
        {
            u16 size = reader.read_16();
            if (debug_mode)
                std::cout << "INIT_ARRAY of size " << size << std::endl;
            runtime::Object **objects = new runtime::Object *[size];
            for (u16 i = 0; i < size; ++i)
            {
                objects[i] = env.stack.top();
                pop();
            }
            runtime::Object *array = env.stack.top();
            pop();
            for (u16 i = 0; i < size; ++i)
            {
                reinterpret_cast<runtime::Link *>(array->data)[i] = objects[i];
            }
            delete[] objects;
            push(array);
            break;
        }
        case Command::INTRINSIC_CALL:
        {
            u16 index = reader.read_16();
            if (debug_mode)
                std::cout << "INTRINSIC_CALL of " << index << std::endl;
            proccess::call_intrinsic(index, env, debug_mode);
            break;
        }
        }
        if (debug_mode)
        {
            std::cout << "Stack size: " << env.stack.size() << std::endl;
            std::cout << "Address " << std::hex << reader.get_offset() << std::endl;
        }
    }
}

void vm::process(const fs::path &file, bool debug_mode)
{
    code::Reader reader(file);
    memory::Allocator allocator;
    Environment env(allocator, parse_header(reader), reader.read_constants(), reader.read_globals(), reader.read_functions(), reader.read_intrinsics());
    u32 length = reader.read_32();
    process(reader, env, length, 0, debug_mode);
}