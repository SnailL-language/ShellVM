#include <gtest/gtest.h>
#include <fstream>
#include <bitset>
#include <iostream>

#include "vm.hpp"

using namespace vm::runtime;

static Object create_int(int value) {
    return {Type::I32, reinterpret_cast<byte *>(new int(value)), 4};
}

TEST(ObjectTests, creationTest)
{
    Object obj = create_int(10);
}

TEST(ObjectTests, boolTest) {
    EXPECT_EQ(true, static_cast<bool>(create_int(1)));
    EXPECT_EQ(false, static_cast<bool>(create_int(0)));
}

static void test_int(int expected) {
    EXPECT_EQ(expected, static_cast<int>(create_int(expected)));
}

TEST(ObjectTests, intTest) {
    test_int(10);
    test_int(30);
    test_int(10000);
    test_int(-10);
    test_int(-105676);
}

TEST(ObjectTests, operatorsTest) {
    Object big = create_int(100000);
    Object small = create_int(-5678);
    EXPECT_TRUE(small < big);
    EXPECT_FALSE(big < small);
    EXPECT_TRUE(small <= big);
    EXPECT_FALSE(big <= small);

    EXPECT_FALSE(small > big);
    EXPECT_TRUE(big > small);
    EXPECT_FALSE(small >= big);
    EXPECT_TRUE(big >= small);
}