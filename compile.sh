#!/bin/bash

gcc -o file_exchange_server file_exchange_server.c
gcc -o file_exchange_client file_exchange_client.c

rm hash.txt 2>/dev/null
rm hash_.txt 2>/dev/null
rm f1.txt_ 2>/dev/null

./file_exchange_server
