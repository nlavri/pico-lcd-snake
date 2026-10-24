# libssh2 Pico adapter

The parent directory contains the source subset needed by this firmware from
libssh2 1.11.1, tag commit `a312b43325e3383c865a87bb1d26cb52e3292641`.
Upstream examples, tests, manuals, CI files, and desktop build systems are
intentionally omitted. The applicable upstream notices are retained in
`../COPYING` and `../LICENSES`.

This adapter builds libssh2 against the Raspberry Pi Pico SDK's mbedTLS and
lwIP libraries. Compression, deprecated APIs, MD5, RIPEMD, RSA/SHA-1, CBC,
3DES, Blowfish, CAST, and RC4 are disabled for the Pico W SSH client.
