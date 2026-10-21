# Compiler commands used by make test.

# Run the official LLVM IR tests by default.
# Override FILTER and STAGE on the command line to select another suite.
FILTER ?= official:codegen
STAGE ?= ir

# Optional: build your compiler once before testing. Leave empty if prebuilt.
BUILD = cmake -S . -B build/cpp && cmake --build build/cpp --parallel 1 \
    && $(CLANG) --target=riscv32-unknown-elf -march=rv32im -mabi=ilp32 \
        -O2 -fno-builtin -S scripts/runtime-example.c \
        -mllvm -riscv-no-aliases -o {runtime}

# Required for semantic tests: exit 0 to accept {source}, 1 to reject it.
SEMANTIC = ./build/cpp/rx_compiler < {source}

# Required for IR tests: write RV32 LLVM IR to {output} (.ll).
IR = ./build/cpp/rx_compiler < {source} > {output}

# Reserved for the student-implemented assembly backend.
CODEGEN =

# Required alongside IR/CODEGEN: run RV32IM assembly with the BUILD runtime.
# Keep program output separate from simulator messages and cycle profiles.
RUN = xmake run -P vendor/REIMU reimu --memory=256M --stack=1M \
    -f {output},{runtime} -o {stdout} -p {profile} 1>&2

