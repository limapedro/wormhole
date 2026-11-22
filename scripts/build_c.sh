#!/bin/sh
# Build and run the all-C Wormhole protocol tests with the platform C compiler.
set -eu
CC=${CC:-cc}
mkdir -p build/c
"$CC" -std=c11 -Wall -Wextra -Werror -O2 -Iinclude \
    core/c/protocol.c tests/test_protocol.c -o build/c/test_protocol
"$CC" -std=c11 -Wall -Wextra -Werror -O2 -Iinclude \
    core/c/protocol.c tests/capture_writer.c -o build/c/capture_writer
build/c/test_protocol
