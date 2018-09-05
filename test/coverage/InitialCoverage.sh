#!/bin/sh
echo "ignore proto and test files"
find build -name "*pb.cc.gcno" -exec rm -rf {} \;
find build -name "*example*.gcno" -exec rm -rf {} \;
find build -name "*unittest*.gcno" -exec rm -rf {} \;
echo "start initialize lcov coverage"
lcov -d build -z
lcov -d build -b . --no-external --initial -c -o rpc_client_c11_init.info

