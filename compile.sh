#!/bin/bash

gcc -o file_server file_server.c
gcc -o file_client file_client.c

rm hash.txt 2>/dev/null
rm hash_.txt 2>/dev/null
rm f1.txt_ 2>/dev/null

./file_server
