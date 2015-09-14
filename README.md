# Introduction

`ldbox` is a lightweight virtualisation environment for crosscompilation.

It allows to transparently build and package software for Linux distributions
and architectures different than the one on a host system.
`ldbox` is lightweight and simple to use.
It requires no additional permissions besides ones a regular user
already has to setup build environment and run commands inside it.

`ldbox` doesn't use any system virtualisation and isolation capabilities
(not even `chroot`).
Instead, it preloads a shared library that modifies arguments to
many functions in `glibc`, the standard C library.
As a result, a program running under `ldbox`'s control sees
a virtual filesystem view according to configurable rules.

Also, these rules control how new programs are started.
For example, a program for foreign architecture can be started inside an
emulator (like `qemu`),
and cross-compiler started instead of native compiler
for foreign architecture.
