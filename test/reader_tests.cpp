#include <gtest/gtest.h>
#include <fstream>
#include <bitset>
#include <iostream>

#include "vm.hpp"

using namespace vm::code;
using namespace vm::runtime;

class ReaderTestFixture : public testing::Test
{
protected:
    Reader reader;

    ReaderTestFixture() : reader("test_data/example.slime") {}

    ~ReaderTestFixture()
    {
        reader.close();
    }
};

TEST_F(ReaderTestFixture, headerTest)
{
    Header header = reader.read_header();
    EXPECT_EQ(0x534E4131, header.magic) << "Reader should read magic number correct!";
    EXPECT_EQ(0x0001, header.version) << "Reader should read version correct!";
    EXPECT_EQ(0x0001, header.main_function_index) << "Reader should read main function index correct!";
}

static void test_const(Type expected_type, byte *expected_value, Object *data)
{
    EXPECT_EQ(expected_type, data->type) << "Type of constant should match expected type!";
    EXPECT_EQ(4U, data->data_size) << "Data size of constant should be 4 bytes!";
    for (int i = 0; i < data->data_size; ++i)
    {
        EXPECT_EQ(expected_value[i], data->data[i]) << "Data of constant should match expected value at index " << i << "!";
    }
}

TEST_F(ReaderTestFixture, constantsTest)
{
    reader.read_header();
    ConstantPool pool = reader.read_constants();
    ASSERT_EQ(11U, pool.size);
    int expected[] = {0, 10, 20, 30, 40, 50, 5, 25, 2, 1, 100};
    Type expected_types[] = {Type::I32, Type::I32, Type::I32, Type::I32, Type::I32, Type::I32, Type::I32, Type::I32, Type::I32, Type::I32, Type::I32};
    for (int i = 0; i < 11; ++i)
    {
        test_const(expected_types[i], reinterpret_cast<byte *>(&expected[i]), pool.data[i]);
    }
}

TEST_F(ReaderTestFixture, globalsTest)
{
    reader.read_header();
    reader.read_constants();
    vm::memory::Allocator allocator;
    GlobalVariables globals = reader.read_globals(allocator);
    ASSERT_EQ(2U, globals.size);
    EXPECT_EQ(nullptr, globals.variables[0].object) << "First global variable shouldn't be initialized here!";
    EXPECT_NE(nullptr, globals.variables[1].object) << "Array global variable should be initialized here!";
}

static void test_function(const Function &function, std::size_t offset, Type return_type, byte arg_count, u16 local_count, u32 length)
{
    EXPECT_EQ(offset, function.offset) << "Function should have offset " << std::hex << offset << "!";
    EXPECT_EQ(return_type, function.return_type) << "Function should return type " << static_cast<int>(return_type) << "!";
    EXPECT_EQ(arg_count, function.arg_count) << "Function should have " << static_cast<int>(arg_count) << " arguments!";
    EXPECT_EQ(local_count, function.local_count) << "Function should have " << local_count << " local variables!";
    EXPECT_EQ(length, function.length) << "Function should have length " << length << " bytes!";
}

TEST_F(ReaderTestFixture, functionsTest)
{
    reader.read_header();
    reader.read_constants();
    vm::memory::Allocator allocator;
    reader.read_globals(allocator);
    FunctionTable functions = reader.read_functions();
    ASSERT_EQ(2U, functions.size);
    test_function(functions.functions[0], 0x0000006C, Type::I32, 2, 4, 30);
    test_function(functions.functions[1], 0x00000097, Type::VOID, 0, 4, 181);
}

static void test_intrinsic(const Intrinsic &intrinsic, Type return_type, byte arg_count, const std::string &name)
{
    EXPECT_EQ(return_type, intrinsic.return_type) << "Intrinsic should return type " << static_cast<int>(return_type) << "!";
    EXPECT_EQ(arg_count, intrinsic.arg_count) << "Intrinsic should have " << static_cast<int>(arg_count) << " arguments!";
    EXPECT_EQ(name, intrinsic.name) << "Intrinsic name should be '" << name << "'!";
}

TEST_F(ReaderTestFixture, intrinsicsTest)
{
    reader.read_header();
    reader.read_constants();
    vm::memory::Allocator allocator;
    reader.read_globals(allocator);
    reader.read_functions();
    IntrinsicTable intrinsics = reader.read_intrinsics();
    ASSERT_EQ(1U, intrinsics.size);
    test_intrinsic(intrinsics.functions[0], Type::VOID, 1, "println");
}