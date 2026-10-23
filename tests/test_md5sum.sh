#!/bin/sh

touch md5sum.txt
touch mymd5sum.txt

test_suite () {
    ACTUAL=0
    EXPECTED=0
    cd ..
    md5sum $@ > tests/md5sum.txt
    EXPECTED=$(echo $?)
    EXPECTED=$((EXPECTED+0))

    ./md5 $@ > tests/mymd5sum.txt
    ACTUAL=$(echo $?)
    ACTUAL=$((ACTUAL+0))

    if [ "$ACTUAL" -ne "$EXPECTED" ]; then
        echo "Failed test: $@"
    fi
    cd tests
    diff 'md5sum.txt' 'mymd5sum.txt'
}

echo "Basics"

echo "Test[1]"
test_suite files/hello.txt
