# templeos-slipstream

Slipstream tool for the TempleOS Standard ISO image

- Appends your ISO.C file to the TempleOS Standard ISO
- Adds a CDirEntry in `/Home` for a `Once.HC` file that automatically mounts your ISO.C file, changes dirs, and `#include`s your `Run.HC` on startup.

This allows you to create slipstreamed TempleOS ISO files that are as close to "pure" as possible, without having to rebuild the distro.

# Usage

`templeos-slipstream TempleOS.ISO MySlipstream.ISO.C output.iso`
