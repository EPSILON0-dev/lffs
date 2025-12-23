#!/bin/sh

if [ ! -f ./fs ]; then
    echo "FS executable not found! Please compile the project first."
    exit 1
fi

if [ $# -ne 4 ]; then
    echo "Usage: $(basename $0) <max_file_size> <block_size> <volume_bytes> <number_of_operations>"
    exit 1
else 
    MAX_FILE_SIZE=$1
    BLOCK_SIZE=$2
    VOLUME_BYTES=$3
    N_OPERATIONS=$4
fi


FS_FILE="tmp/test.lffs"
DB_FILE="tmp/torture_db"
TMP_FILE="tmp/torture_tmp"

random()
{
    echo $(echo "count=0; $(od -An -N4 -tu4 /dev/urandom) % $1" | bc)
}

gen_random_file()
{
    SIZE=$1
    FILE=$2
    dd if=/dev/urandom of=$FILE bs=1 count=$SIZE 2>/dev/null
}

put_file_fs()
{
    fname="file$(head -c2 /dev/urandom | od -An -tu2 | tr -d ' ')"
    fsize=$(random $MAX_FILE_SIZE)
    gen_random_file $fsize $TMP_FILE
    ./fs put $FS_FILE $TMP_FILE $fname
    echo "$fname $fsize $(md5sum $TMP_FILE | cut -d' ' -f1)" >> $DB_FILE
}

delete_file_fs()
{
    line_num=$(random $(wc -l < $DB_FILE))
    fname=$(sed -n "${line_num}p" $DB_FILE | cut -d' ' -f1)
    echo "Deleting $fname"
    ./fs rm $FS_FILE $fname
    sed -i "${line_num}d" $DB_FILE
}

main()
{
    rm -r tmp 2>/dev/null
    mkdir -p tmp
    ./fs create_volume $FS_FILE $BLOCK_SIZE $VOLUME_BYTES

    count=0
    while [ $count -lt $N_OPERATIONS ]; do
        printf "\rOperation %d/%d" $(($count+1)) $N_OPERATIONS
        put_file_fs > /dev/null 2>/dev/null
        put_file_fs > /dev/null 2>/dev/null
        delete_file_fs > /dev/null 2>/dev/null
        count=$((count + 1))
    done
    echo

    echo "Verifying files..."
    cat $DB_FILE | while read line; do
        fname=$(echo $line | cut -d' ' -f1)
        fsize=$(echo $line | cut -d' ' -f2)
        fmd5=$(echo $line | cut -d' ' -f3)
        ./fs get $FS_FILE $fname $TMP_FILE > /dev/null 2>/dev/null
        actual_size=$(stat -c%s $TMP_FILE)
        actual_md5=$(md5sum $TMP_FILE | cut -d' ' -f1)
        if [ "$actual_size" -ne "$fsize" ]; then
            echo "Size mismatch for $fname: expected $fsize, got $actual_size"
        elif [ "$actual_md5" != "$fmd5" ]; then
            echo "MD5 mismatch for $fname: expected $fmd5, got $actual_md5"
        fi
    done

    echo "Final block map:"
    ./fs dump_map $FS_FILE
}

main
