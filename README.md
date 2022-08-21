# URING++

[![License](https://img.shields.io/badge/License-Apache_2.0-blue.svg)](https://opensource.org/licenses/Apache-2.0)

[Documentation](https://liuhaohua.com/uringpp/) | [Examples](https://liuhaohua.com/uringpp/examples.html)

## Building

Requires: C++ compiler with C++20 standard support and `liburing` development headers installed.

```bash
git clone https://github.com/howardlau1999/uringpp && cd uringpp
cmake -Bbuild -DCMAKE_BUILD_TYPE=RelWithDebInfo -DCMAKE_INSTALL_PREFIX=$INSTALL_DIR .
cmake --build build

# To install
cmake --install build
```

## Developing

Install `clang-format` and `pre-commit`. 

```bash
pip install pre-commit
pre-commit install
```