# Upstream

- Project: FreeRTOS Kernel
- Repository: https://github.com/FreeRTOS/FreeRTOS-Kernel
- Version: V11.3.0 (`9b777ae5c5b8e9e456065a00294d1e5f5f9facf5`)
- Imported: 2026-10-23

This is a trimmed local copy containing the kernel core, memory-manager sources,
public headers, and the official GCC RP2040 port required by this firmware. The
application links `heap_4`; the other memory managers are retained because the
upstream RP2040 CMake package declares selectable targets for all of them.
