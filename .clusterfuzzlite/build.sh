#!/bin/bash -eu

# build fuzzers

cmake -Btests_unit/build -Htests_unit -DFUZZ=1 -DCMAKE_C_COMPILER=clang
make -C tests_unit/build/
mv ./tests_unit/build/fuzz_tx $OUT/app-stellar-fuzz-tx