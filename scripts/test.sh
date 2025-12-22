#!/bin/sh

mkdir tmp

echo '--- Creating FS Volume ---'
./fs create_volume tmp/test.lffs 1024 65536

echo '--- Writing Files ---'
./fs put tmp/test.lffs scripts/files/f4k file1
./fs put tmp/test.lffs scripts/files/f2k file2
./fs put tmp/test.lffs scripts/files/f1k file3

./fs get tmp/test.lffs file1 tmp/f4k
./fs get tmp/test.lffs file2 tmp/f2k
./fs get tmp/test.lffs file3 tmp/f1k

echo 'File 1:'
md5sum scripts/files/f4k
md5sum tmp/f4k
echo 'File 2:'
md5sum scripts/files/f2k
md5sum tmp/f2k
echo 'File 3:'
md5sum scripts/files/f1k
md5sum tmp/f1k

echo '--- Testing Fragmentation ---'
./fs rm tmp/test.lffs file2
./fs put tmp/test.lffs scripts/files/f4k file4
./fs put tmp/test.lffs file4 tmp/f4k

echo 'Fragmented file:'
md5sum scripts/files/f4k
md5sum tmp/f4k

echo '--- Cleaning Up ---'
rm -r tmp

