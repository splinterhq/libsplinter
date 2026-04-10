# Building Splinter

Splinter uses CMake, and comes with a `configure` script to manage the various
build options:

```bash
Use: ./configure [options], Where [optiona] are:
--with-lua
--with-wasm
--with-rust
--with-embeddings
--with-numa
--with-llama
```
If you just want KV and embeddings:

```bash
./configure --with-embeddings
```

If you want embeddings and llama:

```bash
./configure --with-embeddings --with-llama
```

If you want to build the whole kitchen sink:

```bash
./configure --dev
```

Next, use `make` and `sudo -E make install` to install the compiled programs and
other build artifacts.
