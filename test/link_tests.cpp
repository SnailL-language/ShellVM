#include <gtest/gtest.h>
#include <fstream>
#include <bitset>
#include <iostream>

#include "vm.hpp"

using namespace vm::runtime;

static Object create_int(int value) {
    return {Type::I32, reinterpret_cast<byte *>(new int(value)), 4};
}

TEST(LinkTests, creationTest) 
{
    Object obj = create_int(20);
    Object *point = &obj;
    Link link;
    link = point;
}