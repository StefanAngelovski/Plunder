# lexbor (vendored)

[lexbor](https://github.com/lexbor/lexbor) is a fast HTML parser with CSS
selector support. Plunder uses it to read the ROM sites' pages (see
`src/scraper/engine`). License: Apache-2.0 (`LICENSE`, `NOTICE`).

`lexbor.c` is lexbor's own single-file build of just the modules Plunder
needs, so every build (device, demo, crawler, tests) compiles one file with
no CMake or downloads. Do not edit it; regenerate it instead:

```sh
# lexbor v3.0.0 source tarball
# https://github.com/lexbor/lexbor/archive/refs/tags/v3.0.0.tar.gz
# sha256 eafaa79ef9871f0bbb1978eda8677d184f7ecdcaa203d7cd25b3f86e32c014c2
tar -xzf lexbor-3.0.0.tar.gz && cd lexbor-3.0.0
perl single.pl html css selectors > lexbor.c   # pulls in core, dom, ns, tag
```

It is only compiled through `src/scraper/engine/htmlq.c`, which includes it
and exposes a small C API (`htmlq.h`) to the rest of Plunder.
