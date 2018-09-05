#!/bin/sh

echo "ignore proto and test files"
find build -name "*pb.cc.gcda" -exec rm -rf {} \;
find build -name "*example*.gcda" -exec rm -rf {} \;
find build -name "*unittest*.gcda" -exec rm -rf {} \;

echo "start report lcov coverage"
lcov -d build -b . --no-external -c -o rpc_client_c11.info
genhtml -o rpc_client_c11_result --prefix=`pwd` rpc_client_c11_init.info rpc_client_c11.info

