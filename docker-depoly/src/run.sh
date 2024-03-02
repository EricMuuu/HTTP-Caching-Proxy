#!/bin/bash
make clean
make
echo 'Proxy Server Starts...'
./proxyServer 12345&
while true ; do continue ; done
