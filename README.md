# imgmake

This repository contains a standalone port of DOSBox-X `imgmake` tool.

# Build

`imgmake` was designed so that it can be built with the ancient Borland C++ 3.1, 
but it is perfectly possible to compile it on any POSIX system. As it is a
single file, building `imgmake` is as simple as invoking:

```sh
$ gcc imgmake.c -D_POSIX_SOURCE -o imgmake
```

If you are using Borland C++ 3.1, load the `imgmake.c` file in the IDE, 
and hit F9 (Make).

# Usage

Run `imgmake -?` to get a detailed help screen.

# Differences

This port of `imgmake` is mostly faithful to the original behavior, however
there are some differences:

- Since it is tailored towards pure MS-DOS systems, FAT32-specific code was 
  removed.

- It is possible to add a label to the created image by specifying a `-label`
  command line option.
  
- Image types (like `fd` or `hd_250`) and command line options are 
  case-insensitive.

- `imgmake` will return a non-zero exit code when image creation is not 
  successful.

# Credits

The DOSBox-X team for the original code, and FreeDOS for the MBR. Both projects
are GPLv2 licensed.
