#include <gtest/gtest.h>
#include <fstream>
#include <bitset>

#include "vm.hpp"

static void test_words(const vm::Reader::word *expected, std::size_t count) {
    fs::path test_file = "test_file.bin";
    std::ofstream tmp(test_file, std::ios::binary);
    tmp.write(reinterpret_cast<const char*>(expected), sizeof(uint16_t) * count);
    tmp.close();

    vm::Reader reader(test_file);

    for (size_t i = 0; i < count; ++i) {
        vm::Reader::word word;
        EXPECT_TRUE(reader.read_word(word)) << "Failed to read word at index " << i;
        EXPECT_EQ(expected[i], word) << "Reader should read the correct word from the file at index " << i << "\n"
                                     << "Expected: " << std::bitset<16>{expected[i]} << ", Got: " << std::bitset<16>{word};
    }

    fs::remove(test_file); // Clean up
}

static void test_bytes(const vm::Reader::byte *expected, std::size_t count) {
    fs::path test_file = "test_file.bin";
    std::ofstream tmp(test_file, std::ios::binary);
    tmp.write(reinterpret_cast<const char*>(expected), sizeof(uint16_t) * count);
    tmp.close();

    vm::Reader reader(test_file);

    vm::Reader::byte bytes[count];
    ASSERT_EQ(count, reader.read_bytes(bytes, count)) << "Failed to read bytes";
    for (int i = 0; i < count; ++i) {
        EXPECT_EQ(expected[i], bytes[i]) << "Reader should read the correct bytes from the file at index " << i << "\n"
                                        << "Expected: " << std::bitset<8>{expected[i]} << ", Got: " << std::bitset<8>{bytes[i]};
    }

    fs::remove(test_file); // Clean up
}

static void test_word(vm::Reader::word expected) {
    test_words(&expected, 1);
}

static void test_byte(vm::Reader::byte expected) {
    test_bytes(&expected, 1);
}

TEST(ReaderTest, singleWordTest) {
    test_word(0x1234);
}

TEST(ReaderTest, multipleWordsTest) {
    vm::Reader::word expected[] = {0x1234, 0x5678, 0x9ABC, 0xDEF0};
    test_words(expected, sizeof(expected) / sizeof(expected[0]));
}

TEST(ReaderTest, singleByteTest) {
    test_byte(0xBA);
}

TEST(ReaderTest, multipleByteTest) {
    vm::Reader::byte expected[] = {0xBA, 0x43, 0x56, 0xCE, 0x21};
    test_bytes(expected, 5);
}