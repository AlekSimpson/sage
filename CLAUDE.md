# Sage Compiler - Agent Reference

## Quick Start

```bash
# Build
make

# Run a file
./build/bin/sage <file.sage>

# Run with bytecode output (useful for debugging)
./build/bin/sage -p <file.sage>

# other debug flags: 
./build/bin/sage -d lexing <file.sage>
./build/bin/sage -d parsing <file.sage>

note: -d and -p can be used together
```

## Makefile Targets

### Building
| Command | Description |
|---------|-------------|
| `make` | Standard build |
| `make debug` | Build with debug symbols (for gdb/lldb) |
| `make release` | Optimized build with -O2 |
| `make memdebug` | Build with AddressSanitizer and LeakSanitizer |
| `make clean` | Remove all build artifacts |
| `make rebuild-debug` | Clean + debug build |
| `make info` | Print build configuration (paths, sources, etc.) |

### Testing
| Command | Description |
|---------|-------------|
| `make test` | Run all tests |
| `make test-update` | Run tests and update expected outputs |
| `make run-test NAME=foo` | Run a specific test by name |
| `make load NAME=foo` | Load a specific test into testfile.sage (for inspection) |
| `make create-test NAME=foo` | Create a new test with given name and source code from testfile.sage |

Tests are managed by `tests/run_tests.jl` (requires Julia).

## Project Structure

- `src/codegen.cpp` - Bytecode generation, visitor functions
- `src/builders.cpp` - Instruction builders, `build_alloca`, `build_store`, `VisitorResult` methods
- `src/compiler.cpp` - Symbol scanning, type resolution, `scan_all_program_symbols`
- `src/symbols.cpp` - Symbol table, type resolution, namespace handling
- `src/interpreter.cpp` - VM execution of bytecode
- `src/parser.cpp` - AST construction from tokens
- `include/codegen.h` - `VisitorResult` struct, `SageCompiler` class

## Key Concepts

### Stack Layout
- **Stack grows DOWN** (high to low addresses)
- Frame pointer (r24) points to base of current frame
- Stack pointer (r23) points to top of stack

### Array Memory Layout
For `array: T[N]`:
```
High addresses
  X          <- old SP
  X-8        <- element[N-1]
  ...
  X-N*8      <- element[0]  <-- first points here
  X-N*8-8    <- struct.first (stores address of element[0])
  X-N*8-16   <- struct.length (stores N)
  X-N*8-16   <- new SP
Low addresses
```

Array access: `first + index * element_size`

### VisitorResult Pattern
`materialize_register()` returns `pair<int64_t, bool>`:
- First: register number OR immediate value
- Second: `true` if immediate, `false` if register

**Important**: Always check the boolean before using the value as a register:
```cpp
auto [value, is_immediate] = result.materialize_register(*this);
int reg;
if (is_immediate) {
    reg = get_volatile_register();
    builder.build_move_immediate(reg, value);
} else {
    reg = value;
}
```

### Common Node Manager Methods
- `get_left/right(node)` - Binary node children
- `get_branch(node)` - Unary node child
- `get_children(node)` - Block node children
- `get_nodetype(node)` - Parse node type (PN_*)
- `get_host_nodetype(node)` - Container type (PN_BINARY, PN_UNARY, PN_TRINARY or PN_BLOCK)

## Address Modes
- `_00`: both operands immediate
- `_01`: first operand immediate, second register
- `_10`: first operand register, second immediate
- `_11`: both operands registers

## Coding Style

### Naming Conventions
- **Types**: `PascalCase` (e.g., `SageArrayType`, `VisitorResult`, `BuiltinNamespace`)
- **Everything else**: `snake_case` (e.g., `array_byte_size`, `visit_array_access`, `current_namespace`)
- **Prefer verbose descriptive names** over abbreviations. Use `array_struct_length_address` not `arr_len_addr`.

### Comments
- Write **high-level comments** that explain non-obvious intent, constraints, or tradeoffs
- Do **not** over-comment - avoid narrating what code obviously does
- Favor **infrequent but highly descriptive** comments for non-trivial sections
- Comments should answer "why" not "what"

```cpp
// Good: Explains non-obvious constraint
// Element area must be above struct in memory for first + index * size addressing to work

// Bad: Narrates obvious code
// Subtract array_byte_size from array_memory_start
builder.build_instruction(OP_SUB, element_start, array_memory_start, array_byte_size, _10);
```
