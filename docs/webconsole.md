# CLeonOS Web Console 文档

`webconsole` 是 CLeonOS 的本机网页控制面板。它和 `httpd` 分离：`httpd` 只部署静态网站，`webconsole` 才负责系统状态、文件浏览、日志查看、网络状态、pkg 包管理和远程命令执行。

## 启动

```sh
webconsole [-p port] [-n max_requests]
```

默认值：

```sh
webconsole -p 8080
```

参数：

- `-p port`：监听端口，默认 `8080`。
- `-n max_requests`：处理指定数量请求后退出；`0` 或不设置表示一直运行。

访问：

```text
http://<CLeonOS-IP>:8080/
```

如果在 CLeonOS 自带 browser 中访问，可以使用：

```sh
browser http://<CLeonOS-IP>:8080/
```

## 页面

导航项：

- `Home` / `System`：系统状态。
- `Files`：文件浏览。
- `Logs`：内核日志查看。
- `Network`：网络状态。
- `Pkg`：包管理面板。
- `Command`：远程执行 CLeonOS 命令。

页面使用非常简单的 HTML/CSS，以便 CLeonOS browser 可以正常渲染。CSS 只保留基础字体、表格边框、`pre` 换行和链接间距。

## System

显示 `cleonos_sys_sysinfo` 返回的系统信息：

- kernel name
- kernel version
- arch
- boot mode
- uptime
- timer ticks / timer Hz
- heap total / used / free
- task count
- service count / ready count

## Files

URL：

```text
/?view=files&path=/
```

功能：

- 点击目录名进入目录。
- 点击文件名预览文件。
- `view`：预览文件前 8192 字节。
- `download`：下载文件。
- `delete`：删除文件或目录。
- `Up`：进入父目录。

限制：

- 路径必须以 `/` 开头。
- 拒绝包含反斜杠 `\` 的路径。
- 拒绝包含 `..` 的路径。
- 文件预览按文本方式显示；二进制文件可能显示乱码。
- 删除操作当前使用 GET 链接触发，适合开发/本机调试，不适合作为公网管理面板暴露。

相关 URL 示例：

```text
/?view=files&path=/shell
/?view=files&action=view&path=/etc/os-release
/?view=files&action=download&path=/etc/os-release
/?view=files&action=delete&path=/temp/test.txt
```

## Logs

URL：

```text
/?view=logs
```

功能：

- 分页查看内核 journal。
- 每页默认 64 条匹配日志。
- 搜索框按关键字过滤日志。
- 翻页时保留搜索关键字。

相关 URL 示例：

```text
/?view=logs
/?view=logs&page=1
/?view=logs&q=TCP
/?view=logs&page=2&q=EXEC
```

## Network

URL：

```text
/?view=net
```

显示：

- network available
- IPv4
- netmask
- gateway
- DNS

这些信息来自内核网络 syscall。

## Pkg

URL：

```text
/?view=pkg
```

功能：

- 安装包：在输入框填写包名，然后点击 `Install`。
- 卸载包：点击已安装包旁边的 `remove`。
- 更新远程包信息：点击 `update`。
- 升级单个包：点击已安装包旁边的 `upgrade`。
- 升级全部包：点击 `upgrade all`。

相关 URL 示例：

```text
/?view=pkg&action=install&name=hello
/?view=pkg&action=remove&name=hello
/?view=pkg&action=update
/?view=pkg&action=upgrade&name=hello
/?view=pkg&action=upgrade
```

实现说明：

- `webconsole` 直接链接 pkg 客户端模块。
- pkg 数据库使用 sqlite。
- JSON API 解析使用 cJSON。
- 构建依赖在 `project.bdt` 的 `app.webconsole.cflags` 和 `app.webconsole.sources` 中声明。
- 不要把这些依赖加回 `app.httpd`。

## Command

URL：

```text
/?view=cmd
```

功能：

- 在网页中填写当前工作目录 `CWD` 和命令行。
- 支持内建远程命令：
  - `pwd`
  - `cd <dir>`
  - `help`
- 支持运行 `/shell/*.elf` 外部命令。
- 标准输出和标准错误会被捕获并显示在网页 `<pre>` 中。
- 单次网页显示最多输出 64 KiB，超出后显示截断提示。

相关 URL 示例：

```text
/?view=cmd&cwd=/&cmd=pwd
/?view=cmd&cwd=/&cmd=ls
/?view=cmd&cwd=/shell&cmd=fastfetch
```

实现说明：

- `webconsole` 使用 `cleonos_sys_exec_pathv_io` 执行外部 ELF。
- 子进程 stdin 绑定到 `/dev/null`，stdout/stderr 捕获到 `/temp/webconsole-<pid>.out`。
- 执行前写入 `USH_CMD_CTX_PATH`，让依赖 User Shell 上下文的外部命令能获取 `cmd`、`arg`、`cwd`。
- 执行后读取 `USH_CMD_RET_PATH`，用于显示命令返回后的工作目录。
- 出于安全考虑，拒绝执行 `/system` 下的程序。
- 当前请求是 GET-only，所以命令内容会出现在 URL、浏览器历史和代理日志中。

## 安全说明

`webconsole` 是本机管理面板，不是公网安全后台。

当前限制：

- 没有 HTTP 登录认证。
- 没有 CSRF 防护。
- 文件删除和 pkg 操作使用 GET 参数触发。
- 没有 HTTPS。
- 单连接轻量 TCP 服务模型。
- `Command` 页面可以远程执行命令。
- `Command` 页面当前使用 GET 表单，命令参数会暴露在 URL 中。

建议：

- 只在本机、虚拟机内网或受信任网络中使用。
- 如果后续需要公网使用，应先增加登录认证、CSRF token、权限检查、POST 表单和 TLS。
- 不要把开启 `Command` 页面的 `webconsole` 暴露到公网。

## 依赖的 syscall

网络服务端：

- `cleonos_sys_net_available`
- `cleonos_sys_net_tcp_listen`
- `cleonos_sys_net_tcp_accept`
- `cleonos_sys_net_tcp_recv`
- `cleonos_sys_net_tcp_send`
- `cleonos_sys_net_tcp_close`

系统状态：

- `cleonos_sys_sysinfo`

文件系统：

- `cleonos_sys_fs_child_count`
- `cleonos_sys_fs_get_child_name`
- `cleonos_sys_fs_stat_type`
- `cleonos_sys_fs_stat_size`
- `cleonos_sys_fs_remove`
- `cleonos_sys_fs_write`
- `cleonos_sys_fs_read`
- `cleonos_sys_fd_open`
- `cleonos_sys_fd_read`
- `cleonos_sys_fd_write`
- `cleonos_sys_fd_close`

远程命令：

- `cleonos_sys_exec_pathv_io`
- `cleonos_sys_getpid`

日志：

- `cleonos_sys_log_journal_count`
- `cleonos_sys_log_journal_read`

网络状态：

- `cleonos_sys_net_ipv4_addr`
- `cleonos_sys_net_netmask`
- `cleonos_sys_net_gateway`
- `cleonos_sys_net_dns_server`

## 源码

- `cleonos/c/apps/webconsole_main.c`
- `cleonos/c/pkg/*`
- `cleonos/third-party/sqlite`
- `cleonos/third-party/cJSON`
- `project.bdt`
