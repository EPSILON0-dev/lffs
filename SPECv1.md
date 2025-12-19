# File System Specification

## File System Structure

### General Information

General file system sructure can be represented as following:

```c
typedef uint32_t FS_LinkTable;
typedef uint8_t FS_DataBlock[4096];

struct FS_Main
{
    FS_SuperBlock super_block;
    // Padding for 4KB alignment
    FS_FileLinkTable link_table[super_block.flt_entry_count];
    // Padding for 4KB alignment
    FS_DataBlock data_blocks[super_block.data_block_count];
};
```

### SuperBlock

The **SuperBlock** is the main filesystem descriptor. It contains the magic value and the version of the file system, as well as information about the organization of the files. The sizes and offsets of **FileLinkingTable** and **DataBlockSection** are also contained in this structure.

```c
struct FS_SuperBlock
{
    char magic[4];
    char version[4];
    uint32_t data_block_size;
    uint32_t data_block_count;
    uint64_t data_block_offset;
    uint64_t flt_offset;
    uint32_t flt_entry_count;
    uint32_t root_block_index;
    uint32_t flags;
    uint32_t reserved[5];
};
```

| Field | Offset | Size | Default Value | Description |
|:-----:|:------:|:----:|:-------------:|-------------|
| **magic** | 0x00 | 4B | **"LFFS"** | The magic value describing the file system. **ALWAYS** set to **"LFFS"** (_0x4C, 0x46, 0x46, 0x53_).|
| **version** | 0x04 | 4B | **0x00000001** |The version the file system. Set to **0x00000001** in the current implementation.|
| **data_block_size** | 0x08 | 4B | **0x1000** | Size of the data block in bytes. The size **MUST** be a power of 2 and **MUST** not be smaller than a single **inode** (no smaller than **32B**). In the current implementation only the size of **4kB** (_0x1000_) is supported. |
| **data_block_count** | 0x0C | 4B | --- |Amount of the **DataBlock**s in the partition. |
| **data_block_offset** | 0x10 | 8B | --- |Offset of the first **DataBlock** from the start of the partition, measured in **bytes**. |
| **data_block_offset** | 0x10 | 8B | --- | Offset of the first **DataBlock** from the start of the partition, measured in **bytes**. **MUST** be aligned to the **data_block_size** boundry. |
| **flt_offset** | 0x18 | 8B | **0x1000** | Offset of the start of the **FileLinkingTable** from the start of the partition, measured in **bytes**. **MUST** be aligned to the **data_block_size** boundry. |
| **flt_entry_count** | 0x20 | 4B | --- | Amount of entries in the **FileLinkingTable**. **SHOULD** be equal to **data_block_count**. |
| **root_block_index** | 0x24 | 4B | **0** | Index of the **DataBlock** containing the root directory. **SHOULD** be **0**. |
| **flags** | 0x28 | 4B | **0** | Flags indicating additional features of the file system. Currently not used, **SHOULD** be **0**. |
| **reserved** | 0x2C | 20B | **{0}** | Reserved for future use. In the current implementation serves as padding to make the **SuperBlock** size a power of 2. |

_NOTE: The version and magic value may be subject to changes._

### FileLinkTable

**FileLinkTable** Contains the links to the next block of the file. The table has **FS_SuperBlock.flt_entry_count** entries. Each entry can be in one of 4 following states:

#### Pointing to the next block

Values: **0x00000001-0x7ffffffe**

The entry with the index of a block points to the index of the next block of that file.

#### Last block

Value: **0x7fffffff**

Indicates that the given block is the last block of a file. _In the future, this value can be changed when appending new blocks to a file without having to erase the block._

#### Free

Value: **0xffffffff**

Indicates that no file resides in a given block.

#### Dirty

Value: **0x00000000**

Indicates that the block was occupied by a file that has been deleted. In the current implementation functionally equivalent to **Free**. _Reserved for future use._

### DataBlock

The **DataBlock** can hold 2 types of data:

#### File Data

**DataBlock** functions as data storage and stores contents of the file. If the file is larger than the block size, the **FileLinkTable** entry corresponding to the current **DataBlock** points to the next **DataBlock** containing the continuation of file data.

#### INode Array

**DataBlock** functions as an array of 128 **INodes**. If there are more **INodes** than a single block can hold, the same mechanism is used as with the files. The **FileLinkTable** entry points to the next **DataBlock** containing more **INodes**. In the current implementation **ONLY** the root **DataBlock** is an **INode** Array.

### INode

**INode** contains information about an entry in the file system. In the current implementation it only describes a file. In future **INode**'s functionality may be extended to also describe directories or symlinks.

```c
struct FS_INode
{
    uint8_t magic;
    uint8_t flags;
    uint8_t extra_inode_count;
    char name[21];
    uint32_t data_block_index;
    uint32_t data_byte_count;
};
```

| Field | Offset | Size | Description |
|:-----:|:------:|:----:|-------------|
| magic | 0x00 | 1B | Type of the **INode**. Possible values are: **0xFF** - empty_node, **0x46** - file node, **0x00** - deleted node. All remaining values are reserved for future use. | 
| flags | 0x01 | 1B | Flags of the **INode**. _Reserved for future use._ |
| extra_inode_count | 0x02 | 1B | Amount of the following **INodes** that provide extra information for this INode. _Reserved for future use._ |
| name | 0x03 | 21B | 21 character ASCII filename. Char array is **NOT** null terminated. |
| data_block_index | 0x18 | 4B | Index of the first **DataBlock** occupied by the referenced file |
| data_byte_count | 0x1C | 4B | Length of the file in **bytes**. |
