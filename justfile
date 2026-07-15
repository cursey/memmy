# Memmy justfile

set windows-shell := ["C:/Program Files/Git/bin/bash.exe", "-cu"]

build_dir := "build"
config := env_var_or_default("CONFIG", "Debug")
clang_format_bin := if os() == "windows" { env_var_or_default("ClangForWindowsBasePath", "C:/Program Files/LLVM") + "/bin/clang-format.exe" } else { "clang-format" }

# List available recipes
default:
    @just --list

# Configure via CMake with Ninja and emit compile_commands.json.
configure:
    cmake -S . -B {{build_dir}} -G Ninja -DCMAKE_BUILD_TYPE={{config}} -DCMAKE_EXPORT_COMPILE_COMMANDS=ON

# Build via CMake.
build: configure
    cmake --build {{build_dir}} --config {{config}}

# Run the CTest suite
test: build
    ctest --test-dir {{build_dir}} -C {{config}} --output-on-failure

# Check the formatting of all first-party C sources and headers
fmt-check:
    find base memmy memmy_ast memmy_eval memmy_cli cmd unittest -type f \( -name '*.c' -o -name '*.h' \) -print0 | xargs -0 "{{clang_format_bin}}" --dry-run --Werror

# Format all first-party C sources and headers
fmt:
    find base memmy memmy_ast memmy_eval memmy_cli cmd unittest -type f \( -name '*.c' -o -name '*.h' \) -print0 | xargs -0 "{{clang_format_bin}}" -i

# Remove build artifacts
clean:
    rm -rf {{build_dir}}
