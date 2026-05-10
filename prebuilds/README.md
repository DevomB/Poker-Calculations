# Prebuilt native addons

End users load **`node.napi.node`** from here via [`node-gyp-build`](https://github.com/prebuild/node-gyp-build). Folder layout:

```
prebuilds/<platform>-<arch>/node.napi.node
```

Examples: `win32-x64`, `darwin-arm64`, `linux-x64`.

These files are **not committed** to git (large binaries). They are produced in CI (`.github/workflows/native-prebuild.yml`) or locally before `npm publish`.

See [README](../README.md) → **Maintainers: publishing**.
