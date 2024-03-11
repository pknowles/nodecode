// Copyright (c) 2024 Pyarelal Knowles, MIT License

#include <cstddef>
#include <cstdint>
#include <cstdlib>
#include <cstring>
#include <gtest/gtest.h>
#include <new>
#include <decodeless/allocator.hpp>
#include <decodeless/header.hpp>
#include <decodeless/offset_ptr.hpp>
#include <type_traits>

using namespace decodeless;

static_assert(std::is_trivially_destructible_v<Version>);
static_assert(std::is_trivially_destructible_v<GitHash>);
static_assert(std::is_trivially_destructible_v<Magic>);
static_assert(std::is_trivially_destructible_v<PlatformBits>);
static_assert(std::is_trivially_destructible_v<Header>);
static_assert(std::is_trivially_destructible_v<RootHeader>);

struct TestRootHeader : RootHeader {
    TestRootHeader()
        : RootHeader("DECODELESS-TEST") {}
};

struct NullAllocator {
    using value_type = std::byte;
    value_type* allocate(std::size_t n) {
        EXPECT_FALSE(allocated);
        allocated = true;
        (void)n;
        return nullptr;
    }
    void deallocate(value_type* p, std::size_t n) noexcept {
        EXPECT_TRUE(allocated);
        (void)p;
        (void)n;
    }
    bool allocated = false;
};

TEST(Version, Invalid) {
    Version a{1, 1, 1};
    Version b;
    EXPECT_FALSE(Version::binaryCompatible(a, b));
    EXPECT_FALSE(Version::binaryCompatible(b, b));
    EXPECT_FALSE(Version::binaryCompatible(b, a));
}

TEST(Version, CompatiblePatch) {
    Version a{2, 2, 1};
    Version b{2, 2, 2};
    Version c{2, 2, 3};
    EXPECT_TRUE(Version::binaryCompatible(a, a));
    EXPECT_TRUE(Version::binaryCompatible(a, b));
    EXPECT_TRUE(Version::binaryCompatible(a, c));
    EXPECT_TRUE(Version::binaryCompatible(b, c));
    EXPECT_TRUE(Version::binaryCompatible(b, a));
    EXPECT_TRUE(Version::binaryCompatible(c, a));
    EXPECT_TRUE(Version::binaryCompatible(c, b));
}

TEST(Version, CompatibleMinor) {
    Version a{2, 1, 2};
    Version b{2, 2, 2};
    Version c{2, 3, 2};
    EXPECT_TRUE(Version::binaryCompatible(a, a));
    EXPECT_FALSE(Version::binaryCompatible(a, b));
    EXPECT_FALSE(Version::binaryCompatible(a, c));
    EXPECT_FALSE(Version::binaryCompatible(b, c));
    EXPECT_TRUE(Version::binaryCompatible(b, a));
    EXPECT_TRUE(Version::binaryCompatible(c, a));
    EXPECT_TRUE(Version::binaryCompatible(c, b));
}

TEST(Version, CompatibleMajor) {
    Version a{1, 2, 2};
    Version b{2, 2, 2};
    Version c{3, 2, 2};
    EXPECT_TRUE(Version::binaryCompatible(a, a));
    EXPECT_FALSE(Version::binaryCompatible(a, b));
    EXPECT_FALSE(Version::binaryCompatible(a, c));
    EXPECT_FALSE(Version::binaryCompatible(b, c));
    EXPECT_FALSE(Version::binaryCompatible(b, a));
    EXPECT_FALSE(Version::binaryCompatible(c, a));
    EXPECT_FALSE(Version::binaryCompatible(c, b));
}

TEST(Allocate, Object) {
    linear_memory_resource<NullAllocator> memory(23);

    // byte can be placed anywhere
    EXPECT_EQ(memory.allocate(sizeof(char), alignof(char)), reinterpret_cast<void*>(0));
    EXPECT_EQ(memory.bytesAllocated(), 1);

    // int after the byte must have 3 bytes padding, placed at 4 and taking 4
    EXPECT_EQ(memory.allocate(sizeof(int), alignof(int)), reinterpret_cast<void*>(4));
    EXPECT_EQ(memory.bytesAllocated(), 8);

    // double after int must have 4 bytes padding, placed at 8, taking 8 more
    EXPECT_EQ(memory.allocate(sizeof(double), alignof(double)), reinterpret_cast<void*>(8));
    EXPECT_EQ(memory.bytesAllocated(), 16);

    // another byte to force some padding, together with another int won't fit
    EXPECT_EQ(memory.bytesReserved() - memory.bytesAllocated(), 7);
    EXPECT_EQ(memory.allocate(sizeof(char), alignof(char)), reinterpret_cast<void*>(16));
    EXPECT_EQ(memory.bytesReserved() - memory.bytesAllocated(),
              6); // plenty left for an int, but not aligned
    EXPECT_THROW((void)memory.allocate(sizeof(int), alignof(int)), std::bad_alloc);
}

TEST(Allocate, Array) {
    linear_memory_resource<NullAllocator> memory(32);

    // byte can be placed anywhere
    EXPECT_EQ(memory.allocate(sizeof(char) * 3, alignof(char)), reinterpret_cast<char*>(0));
    EXPECT_EQ(memory.bytesAllocated(), 3);

    // 2 ints after the 3rd byte must have 3 bytes padding, placed at 4 and taking 8
    EXPECT_EQ(memory.allocate(sizeof(int) * 2, alignof(int)), reinterpret_cast<int*>(4));
    EXPECT_EQ(memory.bytesAllocated(), 12);

    // 2 doubles after 12 bytes must have 4 bytes padding, placed at 16, taking 16 more
    EXPECT_EQ(memory.allocate(sizeof(double) * 2, alignof(double)), reinterpret_cast<double*>(16));
    EXPECT_EQ(memory.bytesAllocated(), 32);
}

TEST(Allocate, Initialize) {
    linear_memory_resource memory(1024);
    std::span<uint8_t>     raw = createArray<uint8_t>(memory, 1024);
    std::ranges::fill(raw, 0xeeu);
    memory.reset();

    int* i = create<int>(memory);
    EXPECT_EQ(i, reinterpret_cast<int*>(raw.data()));
    EXPECT_EQ(*i, 0);

    int* j = create<int>(memory, 42);
    EXPECT_EQ(i + 1, j);
    EXPECT_EQ(*j, 42);

    std::span<int> span = createArray<int>(memory, 10);
    EXPECT_EQ(j + 1, span.data());
    EXPECT_EQ(span[0], 0);

    std::span<int> span2 = createArray(memory, std::vector{0, 1, 2});
    EXPECT_EQ(span2[0], 0);
    EXPECT_EQ(span2[1], 1);
    EXPECT_EQ(span2[2], 2);
}

// Relaxed test case for MSVC where the debug vector allocates extra crap
TEST(Allocate, VectorRelaxed) {
    linear_memory_resource alloc(100);
    EXPECT_EQ(alloc.bytesAllocated(), 0);
    EXPECT_EQ(alloc.bytesReserved(), 100);
    std::vector<uint8_t, linear_allocator<uint8_t>> vec(10, alloc);
    EXPECT_GE(alloc.bytesAllocated(), 10);
    auto allocated = alloc.bytesAllocated();
    vec.reserve(20);
    EXPECT_GT(alloc.bytesAllocated(), allocated);
    EXPECT_THROW(vec.reserve(100), std::bad_alloc);
}

TEST(Allocate, Vector) {
    bool debug =
#if defined(NDEBUG)
        false;
#else
        true;
#endif
    bool msvc =
#if defined(_MSC_VER)
        true;
#else
        false;
#endif
    if (debug && msvc) {
        GTEST_SKIP() << "Skipping test - msvc debug vector makes extraneous allocations";
    }

    linear_memory_resource alloc(30);
    EXPECT_EQ(alloc.bytesAllocated(), 0);
    EXPECT_EQ(alloc.bytesReserved(), 30);

    // Yes, this is possible but don't do it. std::vector can easily reallocate
    // which will leave unused holes in the linear allocator.
    std::vector<uint8_t, linear_allocator<uint8_t>> vec(10, alloc);
    EXPECT_EQ(alloc.bytesAllocated(), 10);
    vec.reserve(20);
    EXPECT_EQ(alloc.bytesAllocated(), 30);
    EXPECT_THROW(vec.reserve(21), std::bad_alloc);
}

TEST(Header, Magic) {
    RootHeader rootHeader("test");
    EXPECT_EQ(rootHeader.decodelessMagic, RootHeader::DecodelessMagic);
    EXPECT_TRUE(rootHeader.magicValid());
    rootHeader.decodelessMagic[10] = 'a';
    EXPECT_FALSE(rootHeader.magicValid());
}

struct Ext1 : Header {
    static constexpr const Magic HeaderIdentifier{"    a"};
    int                    data[10];
};

struct Ext2 : Header {
    static constexpr const Magic HeaderIdentifier{"    b"};
    int                    data[100];
};

TEST(Header, SubHeaders) {
    struct File {
        File()
            : rootHeader("test") {}
        RootHeader         rootHeader;
        Ext1               ext1s[50];
        Ext2               ext2s[50];
        offset_ptr<Header> headers[100];
    };

    File file;
    file.rootHeader.headers = file.headers;
    size_t   nextIndex = 0;
    uint32_t nextId = 123;
    for (auto& e : file.ext1s) {
        file.headers[nextIndex++] = &e;

        // Make this header look like something else to avoid duplicates
        e.identifier.fill(0);
        memcpy(&e.identifier.front(), &++nextId, sizeof(nextId));
    }
    for (auto& e : file.ext2s) {
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

struct AppHeader : decodeless::Header {
    static constexpr Magic   HeaderIdentifier{"APP"};
    static constexpr Version VersionSupported{1, 0, 0};
    AppHeader()
        : decodeless::Header{
              .identifier = HeaderIdentifier, .version = VersionSupported, .gitHash = "unknown"} {}
    decodeless::offset_span<int> data;
};

static_assert(decodeless::SubHeader<AppHeader>);

void writeFile(linear_memory_resource<>& memory, int fillValue) {
    // RootHeader must be first
    TestRootHeader* rootHeader = decodeless::create<TestRootHeader>(memory);

    // Allocate the array of sub-headers
    rootHeader->headers = decodeless::createArray<decodeless::offset_ptr<decodeless::Header>>(memory, 1);

    // Allocate the app header, its data and populate it
    AppHeader* appHeader = decodeless::create<AppHeader>(memory);

    appHeader->data = decodeless::createArray<int>(memory, 100);
    std::ranges::fill(appHeader->data, fillValue);

    // Add the app header the root and sort the array (of one item in this case)
    rootHeader->headers[0] = appHeader;
    std::ranges::sort(rootHeader->headers, RootHeader::HeaderPtrComp());
}

TEST(Header, Readme) {
    // Create a "file"
    linear_memory_resource memory(1000);
    writeFile(memory, 42);
    EXPECT_EQ(memory.bytesAllocated(), 568);

    // "Load" the file; could be memory mapped - no time spent decoding or
    // deserializing!
    auto* root = reinterpret_cast<decodeless::RootHeader*>(memory.arena());

    // Directly access the file, only reading the parts you need
    EXPECT_TRUE(root->binaryCompatible());
    auto* appHeader = root->find<AppHeader>();
    ASSERT_NE(appHeader, nullptr);
    EXPECT_TRUE(
        decodeless::Version::binaryCompatible(AppHeader::VersionSupported, appHeader->version));

    // The offset_span accessor, data points to an arbitrary location in the
    // file, not inside the header
    EXPECT_EQ(appHeader->data[0], 42);
    EXPECT_EQ(appHeader->data.back(), 42);
}
