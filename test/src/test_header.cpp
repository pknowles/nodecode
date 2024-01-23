// Copyright (c) 2024 Pyarelal Knowles, MIT License

#include "nodecode/offset_ptr.hpp"
#include <cstddef>
#include <cstdlib>
#include <nodecode/header.hpp>
#include <nodecode/allocate.hpp>
#include <gtest/gtest.h>
#include <cstring>

using namespace nodecode;

TEST(Version, Invalid) {
    Version a(1, 1, 1);
    Version b;
    EXPECT_FALSE(Version::binaryCompatible(a, b));
    EXPECT_FALSE(Version::binaryCompatible(b, b));
    EXPECT_FALSE(Version::binaryCompatible(b, a));
}

TEST(Version, CompatiblePatch) {
    Version a(2, 2, 1);
    Version b(2, 2, 2);
    Version c(2, 2, 3);
    EXPECT_TRUE(Version::binaryCompatible(a, a));
    EXPECT_TRUE(Version::binaryCompatible(a, b));
    EXPECT_TRUE(Version::binaryCompatible(a, c));
    EXPECT_TRUE(Version::binaryCompatible(b, c));
    EXPECT_TRUE(Version::binaryCompatible(b, a));
    EXPECT_TRUE(Version::binaryCompatible(c, a));
    EXPECT_TRUE(Version::binaryCompatible(c, b));
}

TEST(Version, CompatibleMinor) {
    Version a(2, 1, 2);
    Version b(2, 2, 2);
    Version c(2, 3, 2);
    EXPECT_TRUE(Version::binaryCompatible(a, a));
    EXPECT_FALSE(Version::binaryCompatible(a, b));
    EXPECT_FALSE(Version::binaryCompatible(a, c));
    EXPECT_FALSE(Version::binaryCompatible(b, c));
    EXPECT_TRUE(Version::binaryCompatible(b, a));
    EXPECT_TRUE(Version::binaryCompatible(c, a));
    EXPECT_TRUE(Version::binaryCompatible(c, b));
}

TEST(Version, CompatibleMajor) {
    Version a(1, 2, 2);
    Version b(2, 2, 2);
    Version c(3, 2, 2);
    EXPECT_TRUE(Version::binaryCompatible(a, a));
    EXPECT_FALSE(Version::binaryCompatible(a, b));
    EXPECT_FALSE(Version::binaryCompatible(a, c));
    EXPECT_FALSE(Version::binaryCompatible(b, c));
    EXPECT_FALSE(Version::binaryCompatible(b, a));
    EXPECT_FALSE(Version::binaryCompatible(c, a));
    EXPECT_FALSE(Version::binaryCompatible(c, b));
}

TEST(Allocate, Object) {
    using namespace nodecode::uninitialized;
    std::span<uint8_t> space(reinterpret_cast<uint8_t*>(0), 23);

    // byte can be placed anywhere
    EXPECT_EQ(alignedAllocate<char>(space), reinterpret_cast<char*>(0));
    EXPECT_EQ(space.data(), reinterpret_cast<uint8_t*>(1));

    // int after the byte must have 3 bytes padding, placed at 4 and taking 4
    EXPECT_EQ(alignedAllocate<int>(space), reinterpret_cast<int*>(4));
    EXPECT_EQ(space.data(), reinterpret_cast<uint8_t*>(8));

    // double after int must have 4 bytes padding, placed at 8, taking 8 more
    EXPECT_EQ(alignedAllocate<double>(space), reinterpret_cast<double*>(8));
    EXPECT_EQ(space.data(), reinterpret_cast<uint8_t*>(16));

    // another byte to force some padding, together with another int won't fit
    EXPECT_EQ(space.size(), 7);
    EXPECT_EQ(alignedAllocate<char>(space), reinterpret_cast<char*>(16));
    EXPECT_EQ(space.size(), 6); // plenty left for an int, but not aligned
    EXPECT_THROW(alignedAllocate<int>(space), std::bad_alloc);
}

TEST(Allocate, Array) {
    using namespace nodecode::uninitialized;
    std::span<uint8_t> space(reinterpret_cast<uint8_t*>(0), 32);

    // byte can be placed anywhere
    EXPECT_EQ(alignedAllocate<char>(space, 3).data(), reinterpret_cast<char*>(0));
    EXPECT_EQ(space.data(), reinterpret_cast<uint8_t*>(3));

    // 2 ints after the 3rd byte must have 3 bytes padding, placed at 4 and taking 8
    EXPECT_EQ(alignedAllocate<int>(space, 2).data(), reinterpret_cast<int*>(4));
    EXPECT_EQ(space.data(), reinterpret_cast<uint8_t*>(12));

    // 2 doubles after 12 bytes must have 4 bytes padding, placed at 16, taking 16 more
    EXPECT_EQ(alignedAllocate<double>(space, 2).data(), reinterpret_cast<double*>(16));
    EXPECT_EQ(space.data(), reinterpret_cast<uint8_t*>(32));
}

TEST(Allocate, Initialize) {
    std::vector<max_align_t> fileData(1000);
    std::span<uint8_t> space(reinterpret_cast<uint8_t*>(fileData.data()),
                             fileData.size() * sizeof(*fileData.data()));
    std::ranges::fill(space, 0xeeeeeeee);

    int* i = createLeaked<int>(space);
    EXPECT_EQ(i, reinterpret_cast<int*>(fileData.data()));
    EXPECT_EQ(*i, 0);

    int* j = emplaceLeaked<int>(space, 42);
    EXPECT_EQ(i + 1, j);
    EXPECT_EQ(*j, 42);

    std::span<int> span = createLeaked<int>(space, 10);
    EXPECT_EQ(j + 1, span.data());
    EXPECT_EQ(span[0], 0);

    std::span<int> span2 = emplaceRangeLeaked(space, std::vector{0, 1, 2});
    EXPECT_EQ(span2[0], 0);
    EXPECT_EQ(span2[1], 1);
    EXPECT_EQ(span2[2], 2);
}

TEST(Header, Magic) {
    RootHeader rootHeader;
    EXPECT_EQ(rootHeader.nodecodeMagic, RootHeader::NodecodeMagic);
    EXPECT_TRUE(rootHeader.magicValid());
    rootHeader.nodecodeMagic[10] = 'a';
    EXPECT_FALSE(rootHeader.magicValid());
}

struct Ext1 : Header {
    static constexpr Magic HeaderIdentifier{0, 0, 0, 0, 'a'};
    int data[10];
};

struct Ext2 : Header {
    static constexpr Magic HeaderIdentifier{0, 0, 0, 0, 'b'};
    int data[100];
};

TEST(Header, SubHeaders) {
    struct File {
        RootHeader rootHeader;
        Ext1 ext1s[50];
        Ext2 ext2s[50];
        offset_ptr<Header> headers[100];
    };

    File file;
    file.rootHeader.headers = file.headers;
    size_t nextIndex = 0;
    uint32_t nextId = 123;
    for(auto& e : file.ext1s)
    {
        file.headers[nextIndex++] = &e;

        // Make this header look like something else to avoid duplicates
        e.identifier.fill(0);
        memcpy(&e.identifier.front(), &++nextId, sizeof(nextId));
    }
    for(auto& e : file.ext2s)
    {
        file.headers[nextIndex++] = &e;

        // Make this header look like something else to avoid duplicates
        e.identifier.fill(0);
        memcpy(&e.identifier.front(), &++nextId, sizeof(nextId));
    }

    // RootHeader requires sub-headers to be sorted
    std::ranges::sort(file.headers, RootHeader::HeaderPtrComp());

    // Header identifiers have been corrupted, so they should not be found
    EXPECT_EQ(file.rootHeader.find<Ext1>(), nullptr);
    EXPECT_EQ(file.rootHeader.find<Ext2>(), nullptr);

    // Restore the real identifiers for two headers
    file.ext1s[13].identifier = Ext1::HeaderIdentifier;
    file.ext2s[17].identifier = Ext2::HeaderIdentifier;

    // Must re-sort after changing the identifiers
    std::ranges::sort(file.headers, RootHeader::HeaderPtrComp());

    // Should get back exactly the headers we request
    EXPECT_EQ(file.rootHeader.find<Ext1>(), &file.ext1s[13]);
    EXPECT_EQ(file.rootHeader.find<Ext2>(), &file.ext2s[17]);
}

struct AppHeader : nodecode::Header {
    static constexpr Magic HeaderIdentifier{'A', 'P', 'P'};
    static constexpr Version VersionSupported{1, 0, 0};
    AppHeader()
        : nodecode::Header{.identifier = HeaderIdentifier,
                            .version = VersionSupported} {}
    nodecode::offset_span<int> data;
};

void writeFile(std::span<uint8_t>& space, int fillValue)
{
    // RootHeader must be first
    nodecode::RootHeader* rootHeader = nodecode::createLeaked<nodecode::RootHeader>(space);

    // Allocate the array of sub-headers
    rootHeader->headers =
        nodecode::createLeaked<nodecode::offset_ptr<nodecode::Header>>(space, 1);

    // Allocate the app header, its data and populate it
    AppHeader* appHeader = nodecode::createLeaked<AppHeader>(space);

    appHeader->data = nodecode::createLeaked<int>(space, 100);
    std::ranges::fill(appHeader->data, fillValue);

    // Add the app header the root and sort the array (of one item in this case)
    rootHeader->headers[0] = appHeader;
    std::ranges::sort(rootHeader->headers, RootHeader::HeaderPtrComp());
}

TEST(Header, Readme) {
    // Create a "file"
    std::vector<max_align_t> fileData(1000);
    std::span<uint8_t> space(reinterpret_cast<uint8_t*>(fileData.data()),
                             fileData.size() * sizeof(*fileData.data()));
    writeFile(space, 42);

    // "Load" the file; could be memory mapped - no time spent decoding or
    // deserializing!
    auto* root = reinterpret_cast<nodecode::RootHeader*>(fileData.data());

    // Directly access the file, only reading the parts you need
    EXPECT_TRUE(root->binaryCompatible());
    auto* appHeader = root->find<AppHeader>();
    ASSERT_NE(appHeader, nullptr);
    EXPECT_TRUE(nodecode::Version::binaryCompatible(AppHeader::VersionSupported,
                                                    appHeader->version));

    // The offset_span accessor, data points to an arbitrary location in the
    // file, not inside the header
    EXPECT_EQ(appHeader->data[0], 42);
    EXPECT_EQ(appHeader->data.back(), 42);
}
