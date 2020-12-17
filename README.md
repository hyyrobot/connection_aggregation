# 连接聚合

这个项目用于实现在两个网络主机之间进行连接聚合，将多个网络层连接聚合成一个逻辑连接。

## 开发

### 指令备忘

- `ip`: `iproute2` 提供的网络管理和路由控制指令
  - [仓库](https://github.com/shemminger/iproute2)
  - `ip l` == `ip link`: 查看所有网卡
  - `ip a` == `ip address`: 查看所有网卡及绑定到网卡的 IP 地址
  - `ip r` == `ip route`: 查看静态路由表

- `iperf`/`iperf3` 网络测速工具
  - [官网](https://iperf.fr/)
