#include <gtest/gtest.h>
#include <fstream>
#include <bitset>
#include <iostream>

#include "vm.hpp"

using namespace vm::code;

class ReaderTestFixture : public testing::Test {
protected:
    Reader reader;

    ReaderTestFixture() : reader("test_data/example.slime") {}
   
    ~ReaderTestFixture() {
        reader.close();
    }
};

TEST_F(ReaderTestFixture, headerTest) {
    Header header = reader.read_header();
    EXPECT_EQ(0x534E4131, header.magic) << "Reader should read magic number correct!";
    EXPECT_EQ(0x0001, header.version) << "Reader should read version correct!";
    EXPECT_EQ(0x0001, header.main_function_index) << "Reader should read main function index correct!";
}

static void test_const(byte expected_type, byte *expected_value, byte *data) {
    byte type = data[0];
    EXPECT_EQ(expected_type, type);
    std::size_t count;
    std::size_t offset;
    switch (type)
    {
    case 0x01:
    case 0x02: {
        count = 4;
        offset = 1;
        break;
    }
    case 0x03: {
        count = reinterpret_cast<u16 *>(data + 1)[0];
        offset = 3;
    }
    }
    for (int i = 0; i < count; ++i) {
        EXPECT_EQ(expected_value[i], data[offset + i]);
    }
}

TEST_F(ReaderTestFixture, constantsTest) {
    reader.read_header();
    ConstantPool pool = reader.read_constants();
    ASSERT_EQ(11U, pool.size);
    int expected[] = {0, 10, 20, 30, 40, 50, 5, 25, 2, 1, 100};
    for (int i = 0; i < 11; ++i) {
        test_const(0x01, reinterpret_cast<byte *>(&expected[i]), pool.data[i]);
    }
}