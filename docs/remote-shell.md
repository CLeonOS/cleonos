# CLeonOS Remote Shell

CLeonOS remote shell is a small OpenSSH-like command-line access suite.
It is intentionally not a full SSH implementation yet: the first protocol is plaintext TCP with CLeonOS user login.

## Components

- `/shell/rshd.elf`: CLeonOS-only server.
- `/shell/rsh.elf`: CLeonOS client.
- `tools/cleonos-rsh-client.c`: Linux/Windows host client source.

## Protocol Model

The server accepts one TCP client at a time, asks for username and password, then provides a line-based prompt.
Each submitted line is executed on the server and the captured stdout/stderr is sent back to the client.

Supported server-side built-ins:

- `help`
- `pwd`
- `cd <dir>`
- `exit` or `quit`

Other commands are resolved through the normal CLeonOS shell app path logic, for example `ls` runs `/shell/ls.elf`.

## Usage In CLeonOS

Start the server:

```text
rshd
rshd -p 2222
rshd -p 2222 -n 1
```

Connect from CLeonOS:

```text
rsh 10.0.2.15 2222
```

The default port is `2222`.

## Linux Client Build

```sh
cc -O2 -Wall -Wextra -o cleonos-rsh-client tools/cleonos-rsh-client.c
./cleonos-rsh-client 127.0.0.1 2222
```

If CLeonOS is running under QEMU user networking, expose the guest port with a QEMU hostfwd rule, for example:

```text
-netdev user,id=clksnet0,hostfwd=tcp::2222-:2222
```

Then connect to `127.0.0.1 2222` from the host.

## Windows Client Build

Using MSVC Developer Command Prompt:

```bat
cl /O2 /W3 tools\cleonos-rsh-client.c ws2_32.lib
cleonos-rsh-client.exe 127.0.0.1 2222
```

Using MinGW:

```bat
gcc -O2 -Wall -Wextra -o cleonos-rsh-client.exe tools\cleonos-rsh-client.c -lws2_32
cleonos-rsh-client.exe 127.0.0.1 2222
```

## Security Notes

This is not encrypted SSH. Passwords and command output are plaintext on the network.
Use it only on trusted/local test networks until TLS or a real SSH transport is added.
