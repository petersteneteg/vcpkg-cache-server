# Vcpkg Cache Server
A simple https cache server for [vcpkg](vcpkg.io)

See [Vcpkg binary caching: http provider](https://learn.microsoft.com/en-us/vcpkg/users/binarycaching#http)

## Usage
```bash
Usage: vcpkg_cache_server [--help] [--version] --cache_dir DIR [--port PORT] [--host HOST] [--verbosity LEVEL] [--log_file FILE] [--auth VAR...] [--cert FILE] [--key FILE]

Optional arguments:
  -h, --help       shows help message and exits
  -v, --version    prints version information and exits
  --cache_dir DIR  Directory where to read and write cache [required]
  --port PORT      Port to listen to, defaults to 80 or 443
  --host           Host to listen to [nargs=0..1] [default: "0.0.0.0"]
  --verbosity      Verbosity level 0 (All) to 6 (Off) [nargs=0..1] [default: 2]
  --log_file FILE  Log file, will write with log level 0 (All)
  --auth           List of authentication tokens for write access [nargs: 1 or more] [default: {}]
  --cert FILE      Cert File
  --key FILE       Key File
```

`vcpkg_cache_server` uses the same storage format as the default files provider. 
That is each cached archive is stored as a zip file under `<cache_dir>/<sha[0:2]>/<sha>.zip`.
This means that one can use `vcpkg_cache_server` to serve ones regular `vcpkg` file cache and visa versa.


## Usage from vcpkg:
To use the server in vcpkg add the follwing to ` --binarysource` or the cmake variable `VCPKG_BINARY_SOURCES`
```
http,https://<server>/cache/{sha},readwrite,Authorization: Bearer <write token>
```
or for read only
```
http,https://<server>/cache/{sha},read
```
Replace the `<write token>` with your token and `<server>` with your sever, `{sha}` should be left in place.

## Server config

To enable write access you have to start the server with the `--auth` flag and a list of tokens. 
Multiple tokens can be provided to allow multiple users to write to the cache with individual keys.
Since bearer tokens are passed in plain text it is existential to use https. 
To enable https you have to pass the `--cert` and `--key` parameters. 

## Web API
The server also support a primitive set of queries against the cache to
* `/match` can be use to match vcpkg abi files against entries in the cache, useful for debugging cache misses.
* `/list` list all packages in the cache by name. Will also print the size of each set of packages
* `/find/<package name>` list all caches for a specific package
* `/package/<sha>` list information about a specific cache entry

