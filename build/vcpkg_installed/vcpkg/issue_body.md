Package: bzip2[core,tool]:x64-linux@1.0.8#6

**Host Environment**

- Host: x64-linux
- Compiler: GNU 13.3.0
- CMake Version: 3.31.10
-    vcpkg-tool version: 2026-03-04-4b3e4c276b5b87a649e66341e11553e8c577459c
    vcpkg-scripts version: 751fdf7bbc 2026-03-09 (8 days ago)

**To Reproduce**

`vcpkg install `

**Failure logs**

```
Downloading bzip2-1.0.8.tar.gz, trying https://sourceware.org/pub/bzip2/bzip2-1.0.8.tar.gz
error: curl operation failed with error code 6 (Couldn't resolve host name).
error: Not a transient network error, won't retry download from https://sourceware.org/pub/bzip2/bzip2-1.0.8.tar.gz
Trying https://www.mirrorservice.org/sites/sourceware.org/pub/bzip2/bzip2-1.0.8.tar.gz
error: curl operation failed with error code 6 (Couldn't resolve host name).
error: Not a transient network error, won't retry download from https://www.mirrorservice.org/sites/sourceware.org/pub/bzip2/bzip2-1.0.8.tar.gz
note: If you are using a proxy, please ensure your proxy settings are correct.
Possible causes are:
1. You are actually using an HTTP proxy, but setting HTTPS_PROXY variable to `https://address:port`.
This is not correct, because `https://` prefix claims the proxy is an HTTPS proxy, while your proxy (v2ray, shadowsocksr, etc...) is an HTTP proxy.
Try setting `http://address:port` to both HTTP_PROXY and HTTPS_PROXY instead.
2. If you are using Windows, vcpkg will automatically use your Windows IE Proxy Settings set by your proxy software. See: https://github.com/microsoft/vcpkg-tool/pull/77
The value set by your proxy might be wrong, or have same `https://` prefix issue.
3. Your proxy's remote server is out of service.
If you believe this is not a temporary download server failure and vcpkg needs to be changed to download this file from a different location, please submit an issue to https://github.com/Microsoft/vcpkg/issues
CMake Error at scripts/cmake/vcpkg_download_distfile.cmake:136 (message):
  Download failed, halting portfile.
Call Stack (most recent call first):
  buildtrees/versioning_/versions/bzip2/2d029da682847c5ebdc54e4dbea001331a02207e/portfile.cmake:1 (vcpkg_download_distfile)
  scripts/ports.cmake:206 (include)



```

**Additional context**

<details><summary>vcpkg.json</summary>

```
{
  "name": "vcpkg-cache-server",
  "version-string": "0.0.1",
  "homepage": "https://github.com/petersteneteg/vcpkg-cache-server",
  "license": "BSD-3-Clause",
  "vcpkg-configuration": {
    "default-registry": {
      "kind": "builtin",
      "baseline": "dfb72f61c5a066ab75cd0bdcb2e007228bfc3270"
    }
  },
  "dependencies": [
    {
      "name": "cpp-httplib",
      "features": [
        "brotli",
        "openssl",
        "zlib"
      ]
    },
    {
      "name": "openssl",
      "features": [
        "tools"
      ]
    },
    "libzippp",
    "spdlog",
    "argparse",
    "yaml-cpp",
    "sqlite-orm",
    "rapidfuzz",
    "fmt",
    "fast-float",
    "catch2"
  ]
}

```
</details>
