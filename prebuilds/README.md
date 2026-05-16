# Prebuilt native addons

End users load binaries from here via [`node-gyp-build`](https://github.com/prebuild/node-gyp-build).

## Layout

```
prebuilds/<platform>-<arch>/node.napi.node
prebuilds/<platform>-<arch>/node.napi.musl.node   # optional; Alpine / musl Linux only
```

Examples: `win32-x64`, `linux-x64`, `darwin-arm64`. On **glibc** Linux, `node.napi.node` is used. On **Alpine** (`musl`), the loader prefers **`node.napi.musl.node`** when present in the same folder.

These files are **not committed** to git (large binaries). They are produced in CI ([`.github/workflows/npm-publish.yml`](../.github/workflows/npm-publish.yml)) or locally before `npm publish`.

See [README](../README.md) → **Maintainers: publishing**.
