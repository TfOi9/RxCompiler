# Compiler commands used by make test.

# The project compiler currently supports semantic checking.
# Override FILTER on the command line to select another test suite.
FILTER ?= official:semantic

# Optional: build your compiler once before testing. Leave empty if prebuilt.
BUILD = cmake -S . -B build/cpp && cmake --build build/cpp --parallel 1

# Required for semantic tests: exit 0 to accept {source}, 1 to reject it.
SEMANTIC = ./build/cpp/rx_compiler < {source}

# Required for codegen/optimization tests: compile {source} into {output}.
# To test LLVM IR, write RV32-compatible IR to {output}.ir and append:
#   && clang -S -x ir {output}.ir -o {output} --target=riscv32-unknown-none-elf -march=rv32im -mabi=ilp32 -mllvm -riscv-no-aliases
#   && $(PYTHON) scripts/strip_asm_debug.py {output}
# Fill in once the project compiler's code generation is connected.
CODEGEN =

# Required alongside CODEGEN: run RV32IM assembly in REIMU.
# Keep program output separate from simulator messages and cycle profiles.
RUN = xmake run -P vendor/REIMU reimu --memory=256M --stack=1M \
    -f {output} -o {stdout} -p {profile} 1>&2

