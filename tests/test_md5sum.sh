#!/bin/sh

touch md5sum.txt
touch mymd5sum.txt

test_suite() {
    cd .. || return 1

    md5sum "$@" > tests/md5sum.stdout 2> tests/md5sum.stderr
    EXPECTED=$?

    ./md5 "$@" > tests/mymd5sum.stdout 2> tests/mymd5sum.stderr
    ACTUAL=$?

    if [ "$ACTUAL" -ne "$EXPECTED" ]; then
        echo "Exit status mismatch for: $@"
        echo "Expected: $EXPECTED, Actual: $ACTUAL"
    fi

    if ! diff -u tests/md5sum.stdout tests/mymd5sum.stdout; then
        echo "stdout mismatch for: $@"
    fi

    if ! diff -u tests/md5sum.stderr tests/mymd5sum.stderr; then
        echo "stderr mismatch for: $@"
    fi

    cd tests || return 1
}

echo "Basics"

echo "1"
test_suite files/hello.txt
test_suite files/empty.txt
test_suite files/random_4kb.bin
echo "2"
test_suite -b files/hello.txt
test_suite -b files/empty.txt
test_suite -b files/random_4kb.bin
echo "3"
test_suite -c files/checksum.txt
test_suite -c files/checksum_missing.txt
test_suite -c files/checksum_mismatch.txt
echo "4"
test_suite --tag files/hello.txt
test_suite --tag files/empty.txt
test_suite --tag files/random_4kb.bin
echo "5"
test_suite -t files/hello.txt
test_suite -t files/empty.txt
test_suite -t files/random_4kb.bin
echo "6"
test_suite -z files/hello.txt
test_suite -z files/empty.txt
test_suite -z files/random_4kb.bin
echo "7"
test_suite --help
echo "8"
test_suite --version

echo "Check flags"

echo "9"
test_suite -c --ignore-missing files/checksum.txt
test_suite -c --ignore-missing files/checksum_missing.txt
test_suite -c --ignore-missing files/checksum_mismatch.txt

echo "10"
test_suite -c --quiet files/checksum.txt
test_suite -c --quiet files/checksum_missing.txt
test_suite -c --quiet files/checksum_mismatch.txt

echo "11"
test_suite -c --status files/checksum.txt
test_suite -c --status files/checksum_missing.txt
test_suite -c --status files/checksum_mismatch.txt

echo "12"
test_suite -c --strict files/checksum.txt
test_suite -c --strict files/checksum_missing.txt
test_suite -c --strict files/checksum_mismatch.txt

echo "13"
test_suite -c --warn files/checksum.txt
test_suite -c --warn files/checksum_missing.txt
test_suite -c --warn files/checksum_mismatch.txt

test_suite -c -w files/checksum.txt
test_suite -c -w files/checksum_missing.txt
test_suite -c -w files/checksum_mismatch.txt
