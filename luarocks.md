### Preamble:

Here, `luarocks install lua-parser` fails with:

```
luarocks --lua-version 5.3 install lua-parser
Installing https://luarocks.org/lua-parser-1.0.1-1.rockspec
Cloning into 'lua-parser'...
fatal: remote error:
  The unauthenticated git protocol on port 9418 is no longer supported.
Please see https://github.blog/2021-09-01-improving-git-protocol-security-github/ for more information.

Error: Failed cloning git repository.
```

so we install `lua-parser` manually:

```
git clone https://github.com/andremm/lua-parser
cd lua-parser
luarocks make rockspecs/lua-parser-1.0.1-1.rockspec
```

(if creating the bundled luarocks tree, then pass `--lua-version 5.3 --tree /path/to/bundled/luarocks/`)
