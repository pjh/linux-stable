linux-stable
============

This is pjh's mirror of the linux-stable repo. For now, this is only used for my code changes used for virtual memory tracing (based on linux-3.9.4). See [pjh/vm-analyze](https://github.com/pjh/vm-analyze) for the corresponding tracing and analysis scripts.

Recommended setup steps:

1. Configure:
    - Copy a working config-* file from your /boot directory (e.g. run `uname -r` and grab the config for your current kernel version) to .config in the linux-3.9.4 directory. The .config included in this repo is unlikely to work on other machines.
    - `make oldconfig`
    - Disable unnecessary features if desired...
        * (I usually just disable Paravirtualization - to make kernel build
          faster, and also because Xen build errors arose (at one point,
          not sure if they still do...) with some of my tracing changes.)
1. Build and install; I use these simple steps (but you may wish to find e.g. the suggested Ubuntu steps):
    * `make -j2 &> make.out`
    * `sudo make headers_install`
    * `sudo make modules_install`
    * `sudo make install`
1. Install linux-3.9.4 perf tools to a well-known directory (these steps will install into your home directory):
    * (Note: you may need these apt packages: python-dev)
    * `cd linux-3.9.4/tools/perf`
    * `make`
    * `make prefix=$HOME install`
    * Make sure that $HOME/bin is in your path and `which perf` shows the right version.

