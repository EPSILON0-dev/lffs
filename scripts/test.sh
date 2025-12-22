#!/bin/sh

if [ ! -f ./fs ]; then
    echo "FS executable not found! Please compile the project first."
    exit 1
fi

if [ $# -eq 0 ]; then
    BLOCK_SIZE=1024
else
    BLOCK_SIZE=$1
fi

if ls tmp 1>/dev/null 2>/dev/null; then
    printf '\n--- Cleaning Up Previous Test ---\n'
    rm -r tmp
fi
mkdir -p tmp

printf '\n--- Creating FS Volume ---\n'
./fs create_volume tmp/test.lffs $BLOCK_SIZE 65536

printf '\n--- Writing Files ---\n'
./fs put tmp/test.lffs scripts/files/f4k file1
./fs put tmp/test.lffs scripts/files/f2k file2
./fs put tmp/test.lffs scripts/files/f1k file3

./fs get tmp/test.lffs file1 tmp/f4k
./fs get tmp/test.lffs file2 tmp/f2k
./fs get tmp/test.lffs file3 tmp/f1k

md5sum scripts/files/f4k tmp/f4k scripts/files/f2k tmp/f2k scripts/files/f1k tmp/f1k

printf '\n--- Testing Fragmentation ---\n'
./fs rm tmp/test.lffs file2
./fs put tmp/test.lffs scripts/files/f4k file4
./fs get tmp/test.lffs file4 tmp/f4k
md5sum scripts/files/f4k tmp/f4k

printf '\n--- Oversize File ---\n'
./fs put tmp/test.lffs scripts/files/f1M file_too_large

printf '\n--- Listing Files ---\n'
./fs ls tmp/test.lffs

printf '\n--- Dumping Block Map ---\n'
./fs dump_map tmp/test.lffs

printf '\n--- Cleaning Up ---\n'
./fs delete_volume tmp/test.lffs
rm -r tmp
