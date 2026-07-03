# Memmy justfile

set windows-shell := ["C:/Program Files/Git/bin/bash.exe", "-cu"]

build_dir := "build"
clang_format_bin := if os() == "windows" { env_var_or_default("ClangForWindowsBasePath", "C:/Program Files/LLVM") + "/bin/clang-format.exe" } else { "clang-format" }

# List available recipes
default:
    @just --list

# Configure and build via CMake (uses the system default generator/compiler).
build:
    cmake -S . -B {{build_dir}}
    cmake --build {{build_dir}}

# Format all C sources and headers
fmt:
    find src -name '*.c' -o -name '*.h' | xargs "{{clang_format_bin}}" -i

# Remove build artifacts
clean:
    rm -rf {{build_dir}}
