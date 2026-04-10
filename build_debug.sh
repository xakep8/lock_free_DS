#! /bin/bash

rm -rf out
rm -rf tests/bin
mkdir out && cd out
cmake .. -DCMAKE_BUILD_TYPE=Debug
make -j$(nproc)