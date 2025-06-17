#include <gtest/gtest.h>
#include <fstream>
#include <bitset>
#include <iostream>
#include <vector>

#include "vm.hpp"

using namespace vm::runtime;
using namespace vm::memory;

class AllocatorTestFixture : public ::testing::Test
{
protected:
    Allocator allocator;
};

static Object create_int(int value)
{
    return {Type::I32, reinterpret_cast<byte *>(new int(value)), 4};
}

TEST_F(AllocatorTestFixture, creationTest)
{
    EXPECT_EQ(create_int(30), *allocator.create(Type::I32, reinterpret_cast<byte *>(new int(30)), 4));
}

static Object *create(Allocator &allocator)
{
    return allocator.create(Type::I32, reinterpret_cast<byte *>(new int(30)), 4);
}

static void create(Allocator &allocator, int count)
{
    for (int i = 0; i < count; ++i)
    {
        create(allocator);
    }
}

static std::vector<Object *> create_with_links(Allocator &allocator, int count)
{
    std::vector<Object *> objects;
    for (int i = 0; i < count; ++i)
    {
        Object *obj = create(allocator);
        obj->links++;
        objects.push_back(obj);
    }
    return objects;
}

TEST_F(AllocatorTestFixture, garbageTest)
{
    create(allocator, 10);
    EXPECT_EQ(1, allocator.size());
    std::vector<Object *> objects = create_with_links(allocator, 16);
    EXPECT_EQ(16, allocator.size());
    objects[2]->links = 0;
    objects[5]->links = 0;
    create_with_links(allocator, 2);
    EXPECT_EQ(16, allocator.size());
}