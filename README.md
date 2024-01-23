# nodecode

`nodecode` (no decode) is a general purpose container file format for binary
data, and set of tools to facilitate access. The format definition is just
what's in the C++ headers of this git repository. The philosophy is loading the
file involves no serialization or decoding; data and relative structures within
can be accessed directly.

It's up to the individual app to define what is actually stored. This repository
only defines a top level `RootHeader` which points to a sorted list of
inheritable `Header`s, identified with a unique ID. The intent is to have one
core header that can be extended with optional headers as the need arises, but
again that's up to the app. Some ideas and recommendations when adding concrete
data to headers:

- Use [`offset_ptr` and `offset_span`](https://github.com/pknowles/offset_ptr)
  which remain valid in different address spaces
- Use well defined binary types, e.g. fixed size integers such as `int32_t`
- Align data types appropriately, e.g. using `nodecode::createLeaked()`
- Verify binary compatibility on the current system before reading
- Use a structure of arrays to allow apps to cherry-pick just the data they need

**Example:**

```
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

...

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
```

## Contributing

Issues and pull requests are most welcome, thank you! Note the
[DCO](CONTRIBUTING) and MIT [LICENSE](LICENSE).
