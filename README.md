# LFFS - Lightweight File System

A minimal, purpose-built file system implementation written in C, loosely inspired by FAT (File Allocation Table) file systems. LFFS provides essential file system operations with a simple, efficient architecture optimized for embedded systems and educational use.

## Overview

LFFS is a lightweight file system that manages files in a volume using a File Link Table (FLT) for block allocation and chains, similar in concept to FAT but with a simplified design. The implementation includes:

- **Superblock management** with metadata about the volume
- **File entry structures** for storing file information
- **File Link Table** for tracking block chains
- **Data blocks** for storing file content
- **Command-line interface** for file operations

### On-disk Layout

```
 Byte 0
  |
  v
╔══════════════════════════════════════════╗
║              SUPERBLOCK                  ║
╠══════════════════════════════════════════╣
║  magic[4]          "LFFS"                ║
║  version[4]        "0001"                ║
║  data_block_size   u32                   ║
║  data_block_count  u32                   ║
║  flt_offset        u64  ----------------------> FLT (see below)
║  data_block_offset u64  ----------------------> Data Blocks (see below)
║  flt_entry_count   u32                   ║
║  root_block_index  u32                   ║
║  flags             u32                   ║
║  reserved[5]                             ║
╠══════════════════════════════════════════╣  <-- flt_offset
║         FILE LINK TABLE (FLT)            ║
║         (4 bytes per entry)              ║
╠══════════════════════════════════════════╣
║  [0]: 0x7FFFFFFF  END_OF_CHAIN           ║   root directory block
║  [1]: 0x00000003  next = block 3   ---------. \
║  [2]: 0xFFFFFFFF  FREE                   ║  │  file A: block 1 -> block 3
║  [3]: 0x7FFFFFFF  END_OF_CHAIN    <---------'
║  [4]: 0x00000000  DELETED                ║
║  ...                                     ║
╠══════════════════════════════════════════╣  <-- data_block_offset (block-aligned)
║         DATA BLOCK 0  (root dir)         ║   root_block_index = 0
╠══════════════════════════════════════════╣
║  +------------------------------------+  ║
║  │ FS_FileEntry (32 bytes each)       │  ║
║  │  magic(1) │ flags(1) │ name[21]    │  ║
║  │  data_block_index(u32)             │  ║
║  │  data_byte_count(u32)              │  ║
║  │------------------------------------│  ║
║  │ FS_FileEntry ...                   │  ║
║  +------------------------------------+  ║
╠══════════════════════════════════════════╣
║         DATA BLOCK 1  (file data)        ║   FLT[1] -> block 3
║  [ raw bytes ... ]                       ║
╠══════════════════════════════════════════╣
║         DATA BLOCK 2  (free)             ║   FLT[2] = FREE
╠══════════════════════════════════════════╣
║         DATA BLOCK 3  (file data cont.)  ║   FLT[3] = END_OF_CHAIN
║  [ raw bytes (continued) ... ]           ║
╠══════════════════════════════════════════╣
║         ...                              ║
╚══════════════════════════════════════════╝ EOF
```

## Features

- **Create & Delete Volumes** - Initialize new file systems with configurable block sizes
- **File Operations** - Put, get, remove, and list files
- **Block Management** - Efficient allocation and deallocation of data blocks
- **Chain Tracking** - File Link Table tracks block chains for fragmented files
- **Error Handling** - Comprehensive error codes and diagnostics
- **Testing Suite** - Automated test and torture scripts

## Building the Project

The project uses CMake for building:

```bash
mkdir build
cd build
cmake ..
make
```

This will produce the `fs` executable in the build directory.

## Usage

### Basic Commands

```bash
# Create a new volume with 1024-byte blocks and 65536 bytes total
./fs create_volume <path> <block_size> <bytes>

# List files in a volume
./fs ls <volume>

# Put a file into the volume
./fs put <volume> <src> <dst>

# Get a file from the volume
./fs get <volume> <src> <dst>

# Remove a file from the volume
./fs rm <volume> <filename>

# Delete a volume
./fs delete_volume <path>

# Dump the file block map
./fs dump_map <volume>
```

## Testing

### Basic Test Suite

Run the basic test script to verify core functionality:

```bash
./scripts/test.sh [block_size]
```

The test script:
- Creates a test volume
- Writes and reads multiple files
- Verifies file integrity with MD5 checksums
- Tests fragmentation handling
- Tests capacity limits
- Cleans up test files

Default block size is 1024 bytes; specify an alternative as an argument.

### Stress Testing

Run the torture test for robustness validation:

```bash
./scripts/torture.sh <max_file_size> <block_size> <volume_bytes> <num_operations>
```

Example:
```bash
./scripts/torture.sh 10000 4096 1000000 1000
```

The torture test:
- Generates random files
- Performs random put/delete operations
- Tracks all operations in a database
- Verifies integrity and consistency
- Tests the file system under load

## Design Philosophy

### FAT-Inspired Architecture

LFFS draws inspiration from traditional FAT (File Allocation Table) file systems, which have proven their simplicity and reliability across decades of use. Like FAT, LFFS uses a table-based allocation strategy where:

- **Block chains** are tracked via a link table (similar to FAT's cluster chains)
- **Free block management** uses reserved sentinel values
- **Files can fragment** across non-contiguous blocks
- **Metadata** is kept minimal and efficient

However, LFFS simplifies the FAT model by:
- Using a single unified link table for all blocks (no separate FAT copies)
- Eliminating file name hierarchies (flat file structure)
- Removing complex directory entries
- Reducing per-file metadata overhead


## License

MIT — see [LICENSE](LICENSE).
