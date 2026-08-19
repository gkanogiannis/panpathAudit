# C++ port

Requires CMake 3.20+, a C++20 compiler, and zlib.

```bash
cmake -S cpp -B cpp/build -DCMAKE_BUILD_TYPE=Release
cmake --build cpp/build
ctest --test-dir cpp/build
```

The binary is `cpp/build/panpath-audit-cpp`.

It accepts the same arguments and produces the same reports as the Rust binary.

Sanitizers:

```bash
cmake -S cpp -B cpp/build-san -DPANPATH_SANITIZERS=ON
cmake --build cpp/build-san
ctest --test-dir cpp/build-san
```
