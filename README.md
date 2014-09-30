# sys-open-interceptor
A toy Linux kernel module that intercepts calls to sys_open with a specified extension, and replaces the filename in the call with a filename of your choice. Can be used to replace all audio with Rick Astley for example.

Heavily based on [Julia Evans' work](https://github.com/jvns/kernel-module-fun/blob/master/rickroll.c).
