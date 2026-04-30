# Stage 31: HTTPS/TLS Client Support

## Goal
Add a minimal HTTPS client path for CLeonOS userland using Mbed TLS.

## Implementation
- Added Mbed TLS as `cleonos/third-party/mbedtls`.
- Added a userland TLS adapter in `cleonos/c/apps/tls/`.
- `wget` and `httpget` now accept both `http://` and `https://` URLs.
- HTTPS uses TCP syscall transport as the Mbed TLS BIO backend.
- TLS is configured as a client-only TLS 1.2 profile with SNI and AES-GCM ciphersuites.

## Current Limitations
- Certificate verification is disabled because CLeonOS does not yet ship a CA bundle or trusted wall-clock time.
- TLS entropy uses RDRAND when available and falls back to mixed timer/process state. A real kernel RNG syscall should replace this later.
- `browser` still uses its existing HTTP path.

## Usage
```sh
wget https://example.com/
wget -O /temp/example.html https://example.com/
httpget https://example.com/
```

## Acceptance Criteria
- `wget http://host/path` still works.
- `wget https://host/path` connects with TLS, sends an HTTP/1.1 request, and saves the response body.
- `httpget https://host/path` prints the HTTPS response to stdout.
- `wget.elf` links without host libc TLS/socket dependencies.
