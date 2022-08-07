#!/bin/bash -eu

# build fuzzers

pushd fuzzing
cmake -DCMAKE_C_COMPILER=/usr/bin/clang -DCMAKE_CXX_COMPILER=/usr/bin/clang++ -Bbuild -H.
make -C build
mv ./build/fuzz_tx $OUT/app-stellar-fuzz-tx
popd

