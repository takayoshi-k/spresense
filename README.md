[![twitter spresense handle][]][twitter spresense badge]
[![twitter devworld handle][]][twitter devworld badge]

# Welcome to SPRESENSE project

Clone this repository and update submodules. You can choose from 2 ways in below.

## Shallow clone (Faster, for SDK developers)

```
$ git clone git@github.com:SonySemiconductorSolutions/spresense.git
$ cd spresense
$ git submodule update --init -- nuttx
$ git submodule update --init --depth 1 -- proprietary
$ git submodule foreach git checkout master
```

In this way, `proprietary` repository would be cloned as 'shallow copy'.
Shallow copied repository can be update with `origin/master` only, but can't
refer to other remote branches and old logs.

## Clone completely (Slower)

```
$ git clone --recursive git@github.com:SonySemiconductorSolutions/spresense.git
```

After repositories cloned, each submodules are in 'Detached HEAD'.
So you must checkout master before you getting started.
If you want to all of repository into master, just type like this.

```
$ git submodule foreach git checkout master
```

# Submodules

```
- sdk         - SPRITZER SDK sources and any PC tools
- proprietary - Sony proprietary binaries
- nuttx       - NuttX original kernel + port of SPRITZER architecture
```

# Build

Build instructions are documented at [sdk repository](https://github.com/SonySemiconductorSolutions/spresense/tree/master/sdk).

[twitter spresense handle]: https://img.shields.io/twitter/follow/SpresensebySony?style=social&logo=twitter
[twitter spresense badge]: https://twitter.com/intent/follow?screen_name=SpresensebySony
[twitter devworld handle]: https://img.shields.io/twitter/follow/SonyDevWorld?style=social&logo=twitter
[twitter devworld badge]: https://twitter.com/intent/follow?screen_name=SonyDevWorld
