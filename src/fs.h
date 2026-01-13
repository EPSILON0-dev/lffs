#ifndef __LFFS_H
#define __LFFS_H

#include <stdint.h>
#include <stdio.h>

// ----------------------------------------------------------------------
// File system constants
// ----------------------------------------------------------------------

#define FS_SUPERBLOCK_MAGIC "LFFS0001"

#define FS_FLT_DELETED 0x00000000UL
#define FS_FLT_END_OF_CHAIN 0x7FFFFFFFUL
#define FS_FLT_FREE 0xFFFFFFFFUL

#define FS_FILEMAGIC_FILE 0x46  // 'F'

#define FS_FUN_INLINE static inline __attribute__((always_inline))
#define FS_FUN_INTERNAL static
#define FS_FUN_API

#define FS_FLT_DUMP_ENABLE 1

// ----------------------------------------------------------------------
// File system structures
// ----------------------------------------------------------------------

typedef struct
{
    char magic[4];               // File system magic number
    char version[4];             // File system version
    uint32_t data_block_size;    // Size of each data block in bytes
    uint32_t data_block_count;   // Total number of data blocks
    uint64_t data_block_offset;  // Offset of the first data block in bytes
    uint64_t flt_offset;         // Offset of the file link table in bytes
    uint32_t flt_entry_count;    // Number of entries in the file link table
    uint32_t root_block_index;   // Index of the root directory block
    uint32_t flags;              // File system flags
    uint32_t reserved[5];        // Reserved for future use
} FS_SuperBlock;

typedef struct
{
    uint8_t magic;              // File entry magic number
    uint8_t flags;              // File entry flags
    uint8_t extra_inode_count;  // Reserved for future use
    char name[21];              // File name
    uint32_t data_block_index;  // Index of the first data block
    uint32_t data_byte_count;   // Number of bytes in the data
} FS_FileEntry;

typedef uint32_t FS_FLTEntry;

// ----------------------------------------------------------------------
// Driver specific structures
// ----------------------------------------------------------------------

enum FS_Result
{
    FS_RESULT_OK = 0,
    FS_RESULT_ERROR,
    FS_RESULT_NOT_FOUND,
    FS_RESULT_FILE_EXISTS,
    FS_RESULT_NO_SPACE,
    FS_RESULT_INVALID_PARAMETER,
    FS_RESULT_MOUNT_ERROR,
    FS_RESULT_BROKEN_FLT_CHAIN,
    FS_RESULT_IO_ERROR
};

typedef struct
{
    FILE* file_handle;
    FS_SuperBlock super_block;
    uint32_t used_data_blocks;
    uint32_t file_entry_count;
    uint32_t file_list_block;
    uint32_t file_list_index;
} FS_Volume;

typedef struct
{
    char name[22];
    uint32_t size_bytes;
} FS_FileInfo;

#if FS_FLT_DUMP_ENABLE != 0
typedef struct
{
    int file_count;
    char** file_names;
    uint32_t* file_sizes;
    FS_FLTEntry* entries;  // file_names index + 1, 0 = free
} FS_FLTDump;
#endif

// ----------------------------------------------------------------------
// Driver methods
// ----------------------------------------------------------------------

// Creating and mounting volume
FS_FUN_API int fs_volume_mount(FS_Volume* vol, const char* filepath);
FS_FUN_API int fs_volume_unmount(FS_Volume* vol);
FS_FUN_API int fs_volume_create(
    const char* path, uint32_t blocksz, uint32_t volbytes);

// File access
FS_FUN_API int fs_file_size(
    FS_Volume* vol, const char* name, uint32_t* bytes);
FS_FUN_API int fs_file_read(FS_Volume* vol, const char* name, void* buff,
    uint32_t buffsz, uint32_t* br);
FS_FUN_API int fs_file_write(FS_Volume* vol, const char* name,
    const void* buff, uint32_t buffsz, uint32_t* bw);
FS_FUN_API int fs_file_delete(FS_Volume* vol, const char* name);

// Retrieving file information
FS_FUN_API int fs_list_len(FS_Volume* vol, uint32_t* count);
FS_FUN_API int fs_list_get(FS_Volume* vol, FS_FileInfo* info);
FS_FUN_API int fs_list_reset(FS_Volume* vol);

// Error string
FS_FUN_API const char* fs_strerror(int code);

// FLT Dumping
#if FS_FLT_DUMP_ENABLE != 0
FS_FUN_API int fs_flt_dump_create(FS_Volume* vol, FS_FLTDump* dump);
FS_FUN_API int fs_flt_dump_free(FS_FLTDump* dump);
#endif

#endif
