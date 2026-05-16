# CLeonOS httpd 文档

`httpd` 是 CLeonOS 的简单静态网站部署器。它只负责把指定目录下的文件通过 HTTP GET 发送给客户端，不包含系统控制面板、pkg 管理、日志查看等动态功能。

动态网页面板请使用 `webconsole`，见 `docs/webconsole.md`。

## 启动

```sh
httpd [-p port] [-r root] [-n max_requests]
```

默认值：

```sh
httpd -p 80 -r /www
```

参数：

- `-p port`：监听端口，默认 `80`。
- `-r root`：网站根目录，默认 `/www`。
- `-n max_requests`：处理指定数量请求后退出；`0` 或不设置表示一直运行。

示例：

```sh
httpd -p 80 -r /www
httpd -p 8080 -r /home/root/site
httpd -p 8080 -r /www -n 10
```

## 请求行为

- 只支持 `GET`。
- URL `/` 会映射到 `root/index.html`。
- URL query 会被忽略，例如 `/index.html?v=1` 仍读取 `/index.html`。
- 不支持目录列表。
- 不支持 POST、PUT、DELETE。
- 不支持 CGI、PHP、server-side script。
- 不支持 TLS/HTTPS；如需 HTTPS，需要在其它层实现 TLS 终止或单独写支持 TLS 的服务端。

## MIME 类型

当前根据文件后缀返回基础 `Content-Type`：

- `.html` / `.htm`：`text/html; charset=utf-8`
- `.txt` / `.log`：`text/plain; charset=utf-8`
- `.css`：`text/css; charset=utf-8`
- `.js`：`application/javascript`
- `.png`：`image/png`
- `.jpg` / `.jpeg`：`image/jpeg`
- `.gif`：`image/gif`
- 其它：`application/octet-stream`

## 安全限制

`httpd` 会拒绝危险路径：

- 路径必须以 `/` 开头。
- 拒绝包含反斜杠 `\` 的路径。
- 拒绝包含 `..` 的路径。

这用于避免客户端通过 `../` 逃出网站根目录。

## 实现边界

`httpd` 不链接 pkg、sqlite、cJSON，也不访问控制台 API。这样可以保持体积小、依赖少、行为稳定。

相关源码：

- `cleonos/c/apps/httpd_main.c`
- `project.bdt` 中没有 `app.httpd.sources` 重依赖配置，走普通应用自动构建。

## 依赖的 syscall

网络：

- `cleonos_sys_net_available`
- `cleonos_sys_net_tcp_listen`
- `cleonos_sys_net_tcp_accept`
- `cleonos_sys_net_tcp_recv`
- `cleonos_sys_net_tcp_send`
- `cleonos_sys_net_tcp_close`

文件系统：

- `cleonos_sys_fs_stat_type`
- `cleonos_sys_fs_stat_size`
- `cleonos_sys_fd_open`
- `cleonos_sys_fd_read`
- `cleonos_sys_fd_close`

## 常见问题

### 浏览器显示 Not Found

检查：

- `-r` 指定的根目录是否存在。
- 访问 `/` 时根目录下是否有 `index.html`。
- 文件路径是否大小写正确。

### listen failed

可能原因：

- 网络不可用。
- 端口无效。
- TCP 栈当前已有未关闭会话或端口状态未释放。

### 访问控制面板失败

`httpd` 已不再内置控制面板。请运行：

```sh
webconsole -p 8080
```

