# Overview of the source code 

The following illustrates the content of the root directory: 

```
.
├── LICENSE
├── README.md
├── RELEASE_NOTES.md
├── xmake.lua
├── .clang-format
├── .github
├── benchmarks
├── blog
├── ci
├── doc
├── xmake
├── gamegine
├── tests
└── third_party
```

- [LICENSE](../LICENSE) is the LICENSE of the project (Apache 2.0). 
- [README.md](../README.md) contians the general information of the project. 
- [RELEASE_NOTES.md](../RELEASE_NOTES.md) contains the detail of what is included in each release of the engine. 
- [xmake.lua](../xmake.lua) is the build script of the engine. 
- [.clang-format](../.clang-format) is the formatting contract (BDE style, 79 columns), enforced by `xmake ci-format`. 
- [.github](../.github/) contains the GitHub Actions workflows; `push.yml` is the push and pull-request tier described in [ci.md](ci.md). 

###

- [benchmarks](/benchmarks/) contains the benchmarks of the engine. 
- [blog](/blog/) contains the raw typst sources of our blog files on the engine internals working. 
- [ci](/ci/) contains the CI tooling: pinned inputs, the frozen manifest schema, and the xmake tasks that enforce them. See [ci/README.md](../ci/README.md). 
- [doc](/doc/) contains the general engine documentation. 
- [gamegine](/gamegine/) contains the engine library. 
- [tests](/tests/) contains the tests of the engine library. 
- [third_party](/third_party/) contains the headers of third party we use, including `clusterlod.h`, vendored from the upstream meshoptimizer `demo/` directory and pinned by content hash. 
- [xmake](/xmake/) contains local xmake package definitions for dependencies pinned to an exact upstream commit rather than to a published release. 
