#include "vm.hpp"

#include <filesystem>
#include <fstream>
#include <sstream>
#include <iostream>
#include <dlfcn.h>

using namespace vm;
using Command = vm::code::Command;

static void write_header(std::ofstream &source, const std::string &func_name)
{
    source << "#include \"vm.hpp\"\n"
           << "#include <vector>\n"
           << "#include <stack>\n"
           << "#include <string>\n"
           << "#include <iostream>\n"
           << "#include <functional>\n"
           << '\n'
           << "using namespace vm;\n"
           << '\n'
           << "extern \"C\" void " << func_name << "(code::Reader &reader,\n"
           << "    Environment &env,\n"
           << "    std::function<void(runtime::Object *)> push,\n"
           << "    std::function<void()> pop,\n"
           << "    std::function<void(const char *, std::function<int(int &&, int &&)>, std::function<u32(u32 &&, u32 &&)>, std::function<std::string(std::string &&, std::string &&)>)> arithmetic_operation,\n"
           << "    std::function<void(const char *, std::function<bool(int &&, int &&)>, std::function<bool(u32 &&, u32 &&)>)> compare_operation,\n"
           << "    std::function<void(const char *, std::function<bool(bool &&, bool &&)>)> logical_operation,\n"
           << "    bool debug_mode)\n"
           << "{\n";
}

void jit::compile_func(code::Reader &reader, int id, code::Function &function, bool debug_mode)
{
    std::string source_path = std::filesystem::temp_directory_path().append("jit_func_").string().append(std::to_string(id));
    std::ofstream source(source_path + ".cpp", std::ios::trunc);
    if (!debug_mode)
        std::cout << "Write code to " << source_path << std::endl;
    std::stringstream ss;
    ss << "jit_func_" << id;
    write_header(source, ss.str());
    source << "std::vector<runtime::Link> local_variables(" << function.arg_count + function.local_count << ");\n";
    source << "int result;\n";
    source << "runtime::Object *array, *index, *value, *condition_obj;\n";
    auto write_push = [&reader, &source, debug_mode](const char *suffix, const char *getter_start, const char *getter_end)
    {
        u16 index = reader.read_16();
        if (debug_mode)
            source << "    std::cout << \"PUSH_ " << suffix << " from index \" << " << index << " << std::endl;\n";
        source << "push(" << getter_start << index << getter_end << ");\n";
    };
    auto write_store = [&reader, &source, debug_mode](const char *suffix, const char *getter_start, const char *getter_end)
    {
        u16 index = reader.read_16();
        if (debug_mode)
            source << "    std::cout << \"STORE_" << suffix << " to index " << index << "\" << std::endl;\n";
        source << getter_start << index << getter_end << " = env.stack.top();\n"
               << "pop();\n";
    };
    auto write_jump_if = [&reader, &source, debug_mode](bool condition)
    {
        int length = static_cast<std::int16_t>(reader.read_16());
        source << "condition_obj = env.stack.top();\n"
               << "pop();\n";
        if (debug_mode)
            source << "std::cout << \"JUMP_IF_" << (condition ? "TRUE" : "FALSE") << " to " << length << "\" << std::endl;\n";
        source << "if (" << (condition ? "true" : "false") << " == static_cast<bool>(*condition_obj))\n"
               << "    goto mark" << reader.get_offset() + length << ";\n";
    };
    std::size_t start = reader.get_offset();
    while (reader.get_offset() - start < function.length)
    {
        source << "mark" << reader.get_offset() << ":\n";
        byte command = reader.read_byte();
        switch (command)
        {
        case Command::PUSH_CONST:
        {
            write_push("CONST", "env.constant_pool.data[", "]");
            break;
        }
        case Command::PUSH_LOCAL:
        {
            write_push("LOCAL", "local_variables[", "].object");
            break;
        }
        case Command::PUSH_GLOBAL:
        {
            write_push("GLOBAL", "env.global.variables[", "].object");
            break;
        }
        case Command::STORE_LOCAL:
        {
            write_store("LOCAL", "local_variables[", "]");
            break;
        }
        case Command::STORE_GLOBAL:
        {
            write_store("GLOBAL", "env.global.variables[", "]");
            break;
        }
        case Command::POP:
        {
            if (debug_mode)
                source << "std::cout << \"POP \" << std::endl;\n";
            source << "pop();\n";
            break;
        }
        case Command::DUP:
        {
            if (debug_mode)
                source << "std::cout << \"DUP \" << std::endl;\n";
            source << "push(env.stack.top());\n";
            break;
        }

        case Command::ADD:
        case Command::SUB:
        case Command::MUL:
        case Command::DIV:
        case Command::MOD:
        {
            source << "arithmetic_operation(\n"
                   << '"' << (command == Command::ADD ? "ADD" : command == Command::SUB ? "SUB"
                                                            : command == Command::MUL   ? "MUL"
                                                            : command == Command::DIV   ? "DIV"
                                                                                        : "MOD")
                   << "\",\n"
                   << "proccess::get_arithmetic_function<int>(" << (int)command << "),\n"
                   << "proccess::get_arithmetic_function<u32>(" << (int)command << "),\n"
                   << "[](std::string &&a, std::string &&b)\n"
                   << "    { return a + b; });\n";
            break;
        }

        case Command::EQ:
        case Command::NEQ:
        case Command::LT:
        case Command::LE:
        case Command::GT:
        case Command::GTE:
        {
            source << "compare_operation(\n"
                   << '"' << (command == Command::EQ ? "EQ" : command == Command::NEQ ? "NEQ"
                                                          : command == Command::LT    ? "LT"
                                                          : command == Command::LE    ? "LE"
                                                          : command == Command::GT    ? "GT"
                                                                                      : "GTE")
                   << "\",\n"
                   << "proccess::get_comparison_function<int>(" << (int)command << "),\n"
                   << "proccess::get_comparison_function<u32>(" << (int)command << "));\n";
            break;
        }
        case Command::AND:
        case Command::OR:
        {
            source << "logical_operation(\n"
                   << '"' << (command == Command::AND ? "AND" : "OR") << "\",\n"
                   << "proccess::get_logical_function(" << (int)command << "));\n";
            break;
        }
        case Command::NOT:
        {
            source << "runtime::Object *obj = env.stack.top();\n"
                   << "pop();\n";
            if (debug_mode)
                source << "std::cout << \"Not of \" << (int)*obj << std::endl;\n";
            source << "result = !static_cast<bool>(*obj);\n"
                   << "push(env.allocator.create(runtime::Type::I32, reinterpret_cast<byte *>(&result), 4));";
            break;
        }

        case Command::JMP:
        {
            int length = static_cast<std::int16_t>(reader.read_16());
            if (debug_mode)
                source << "std::cout << \"JMP to \" << " << length << " << std::endl;\n";
            source << "goto mark" << reader.get_offset() + length << ";\n";
            break;
        }
        case Command::JMP_IF_FALSE:
        case Command::JMP_IF_TRUE:
        {
            write_jump_if(command == Command::JMP_IF_TRUE);
            break;
        }
        case Command::CALL:
        {
            u16 index = reader.read_16();
            std::string func_name = "func" + std::to_string(reader.get_offset());
            source << "code::Function " << func_name << " = env.functions.functions[" << index << "];\n";
            if (debug_mode)
                source << "std::cout << \"CALL of " << index << "\" << std::endl;";
            source << "if (" << func_name << ".calls++ > 100)\n"
                   << "{\n"
                   << "    if (" << func_name << ".compiled == nullptr)\n"
                   << "    {\n"
                   << "        reader.set_offset(" << func_name << ".offset);\n"
                   << "        jit::compile_func(reader, " << index << ", " << func_name << ", debug_mode);\n"
                   << "    }\n"
                   << "    reinterpret_cast<proccess::jit_function *>(" << func_name << ".compiled)(reader, env, push, pop, arithmetic_operation, compare_operation, logical_operation, debug_mode);\n"
                   << "}\n"
                   << "else\n"
                   << "{\n"
                   << "    reader.set_offset(" << func_name << ".offset);\n"
                   << "    process(reader, env, " << func_name << ".length, " << func_name << ".local_count + " << func_name << ".arg_count, debug_mode);\n"
                   << "}\n";
            break;
        }
        case Command::RET:
        {
            if (debug_mode)
                source << "std::cout << \"RET\" << std::endl;\n";
            source << "return;\n";
            break;
        }
        case Command::HALT:
        {
            if (debug_mode)
                source << "std::cout << \"HALT\" << std::endl;";
            source << "throw runtime::HaltException(\"HALT command found in bytecode!\");\n";
            break;
        }

        case Command::NEW_ARRAY:
        {
            u32 size = reader.read_32();
            reader.skip(1U);
            if (debug_mode)
                source << "std::cout << \"NEW_ARRAY of " << size << " elements" << "\" << std::endl;\n";
            source << "push(env.allocator.create(runtime::Type::ARRAY, nullptr, " << size << "));\n";
            break;
        }
        case Command::GET_ARRAY:
        {
            source << "index = env.stack.top();\n"
                   << "pop();\n"
                   << "array = env.stack.top();\n"
                   << "pop();\n";
            if (debug_mode)
                source << "std::cout << \"GET_ARRAY in \" << (int)*index << std::endl;\n";
            source << "push(reinterpret_cast<runtime::Link *>(array->data)[static_cast<u32>(*index)].object);\n";
            break;
        }
        case Command::SET_ARRAY:
        {
            source << "index = env.stack.top();\n"
                   << "pop();\n"
                   << "value = env.stack.top();\n"
                   << "pop();\n"
                   << "array = env.stack.top();\n"
                   << "pop();\n";
            if (debug_mode)
                source << "std::cout << \"SET_ARRAY in \" << static_cast<u32>(*index) << \" with \" << static_cast<int>(*value) << std::endl;";
            source << "reinterpret_cast<runtime::Link *>(array->data)[static_cast<u32>(*index)] = value;";
            break;
        }
        case Command::INIT_ARRAY:
        {
            u16 size = reader.read_16();
            if (debug_mode)
                source << "std::cout << \"INIT_ARRAY of size " << size << "\" << std::endl;";

            source << "runtime::Object *objects" << reader.get_offset() << "[" << size << "];\n"
                   << "for (u16 i = 0; i < " << size << "; ++i)\n"
                   << "{\n"
                   << "    objects" << reader.get_offset() << "[i] = env.stack.top();\n"
                   << "    pop();\n"
                   << "}\n"
                   << "array = env.stack.top();\n"
                   << "pop();\n"
                   << "for (u16 i = 0; i < " << size << "; ++i)\n"
                   << "{\n"
                   << "    reinterpret_cast<runtime::Link *>(array->data)[i] = objects" << reader.get_offset() << "[i];\n"
                   << "}\n"
                   << "push(array);\n";
            break;
        }
        case Command::INTRINSIC_CALL:
        {
            u16 index = reader.read_16();
            if (debug_mode)
                source << "std::cout << \"INTRINSIC_CALL of " << index << "\" <<  std::endl;\n";

            source << "proccess::call_intrinsic(" << index << ", env, debug_mode);\n";
            break;
        }
        }
    }
    source << "}\n";
    source.close();

    if (debug_mode)
        std::cout << "Compile generated code" << std::endl;

    system(("clang++ -std=c++20 -c -fPIC -I /Users/alexey/SnailL-dev/vm/include -o " + source_path + ".o " + source_path + ".cpp").c_str());
    system(("clang++ -shared -L/Users/alexey/SnailL-dev/build -lvm -o " + source_path + ".dylib " + source_path + ".o").c_str());

    auto lib = dlopen((source_path + ".dylib").c_str(), RTLD_NOW | RTLD_LOCAL);
    function.compiled = dlsym(lib, ss.str().c_str());
}