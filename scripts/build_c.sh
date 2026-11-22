#!/bin/sh
# Build and run the all-C Wormhole protocol tests with the platform C compiler.
set -eu
CC=${CC:-cc}
mkdir -p build/c
"$CC" -std=c11 -Wall -Wextra -Werror -O2 -Iinclude \
    core/c/protocol.c tests/test_protocol.c -o build/c/test_protocol
"$CC" -std=c11 -Wall -Wextra -Werror -O2 -Iinclude \
    core/c/protocol.c tests/capture_writer.c -o build/c/capture_writer
"$CC" -std=c11 -Wall -Wextra -Werror -O2 -Iinclude -Ivendor \
    core/c/protocol.c core/c/harness.c core/c/proxy.c core/c/quality.c core/c/differential.c \
    core/c/media.c core/c/reference.c core/c/harness_main.c -o build/c/wormhole-harness -lm
"$CC" -std=c11 -Wall -Wextra -Werror -O2 -Iinclude \
    core/c/protocol_v2.c tests/test_protocol_v2.c -o build/c/test_protocol_v2
"$CC" -std=c11 -Wall -Wextra -Werror -O2 -Iinclude -D_POSIX_C_SOURCE=200809L \
    core/c/protocol_v2.c tests/test_v2_negotiation.c -o build/c/test_v2_negotiation
build/c/test_protocol
build/c/test_protocol_v2
build/c/test_v2_negotiation
