# 日志

## 目录

- [20201209 对需求和技术的一些认识](#[20201209]-对需求和技术的一些认识)
- [20201210 如何绑定网卡/确定实现方案](#[20201210]-如何绑定网卡/确定实现方案)
- [20201214 在 VirtaulBox 中配置一个具有一定结构的网络进行测试](#[20201214]-在-VirtaulBox-中配置一个具有一定结构的网络进行测试)
- [20201222 原来是这样](#[20201222]-原来是这样)
- [20201223 流水账](#[20201223]-实现连接监视、设计协议)
- [20201224 实现包收发、设计转发协议](#[20201224]-实现包收发、设计转发协议)

## [20201209] 对需求和技术的一些认识

- 初步的需求
  - 通过多链路冗余传输提高音视频和其他数据的传输可用性。使用可用性这个词，指的是不但要到达，而且要在较小的时延下到达。
  - 服务本身需要是透明的，以兼容 WebRTC 等内置网络功能的框架，也就是说服务应该伪装成一个操作系统已有的机制（而不是一个库），且不需要应用程序在数据中添加控制信息。

- 确定的条件
  - 部署服务的主机必然是 Ubuntu18 或 Ubuntu20；

- 使用到的技术：
  - 基于 TUN 虚拟三层网络设备；
  - 使用 Netlink 配置到操作系统；

---

- 一些资源：

  - [为什么 Linux 自带的多网卡绑定模式不适用](https://blog.51cto.com/2979193/2095134)
  - [RtNetlink 中既无注释又无文档的宏都是什么意思](https://wenku.baidu.com/view/fe88c54469eae009581bec8d.html)
  - [另一个对 RtNetlink 的说明](https://www.cnblogs.com/wenqiang/p/6634447.html)

## [20201210] 如何绑定网卡/确定实现方案

**摘要**：在 Linux 上，bind ≠ 绑定网卡，Linux 一定会查路由表然后选择一个IP 地址/网卡发送。要指定网卡，只能通过设定 SO_BINDTODEVICE 套接字选项。

- 资料：
  - [如何绑定1](https://code-examples.net/en/q/3d0369)
  - [如何绑定2](https://blog.csdn.net/x356982611/article/details/80196424)
  - [基于 SO_BINDTODEVICE 工作原理的分析](https://blog.51cto.com/dog250/1271769)

- 获得一系列具有指定本地 ip 地址，固定从指定网卡收发的原始套接字：

  ```c++
  // 使用 netlink 获得所有端口上的所有 ip 地址
  // networks := std::unordered_map<link_index, {link_name, std::vector<{address, subnet_length}>}>
  auto networks = list_networks(fd_netlink);
  char address[17];
  // 遍历端口
  for (auto p = networks.begin(); p != networks.end();)
      // 排除回环地址和 tun 设备本身
      if (p->first == 1 || p->first == tun_index)
          p = networks.erase(p);
      else
          ++p;
  // 用于保存套接字的 std::vector
  sockets.resize(networks.size());
  constexpr static auto on = 1;
  auto p = networks.begin();
  for (auto i = 0; i < sockets.size(); ++i, ++p)
  {
      const auto fd = sockets[i] = socket(AF_INET, SOCK_RAW, IPPROTO_RAW);                 // 建立一个网络层原始套接字
      setsockopt(fd, IPPROTO_IP, IP_HDRINCL, &on, sizeof(on));                             // 指定协议栈不再向发出的分组添加 ip 头
      setsockopt(fd, SOL_SOCKET, SO_BINDTODEVICE, p->second.name, strlen(p->second.name)); // 绑定套接字到网卡
      sockaddr_in localaddr = {
          .sin_family = AF_INET,
          .sin_port = 0,
          .sin_addr = p->second.addresses.front().address,
      };
      bind(fd, (sockaddr *)&localaddr, sizeof(localaddr)); // 绑定套接字 ip 地址
  }
  ```
 
---

目前设定的方案是实现为特殊第四层协议，使用第三层原始套接字直接拦截所有网络口上的输入输出 IP 数据报，在 TUN 中修改其 IP 头，将附加信息也写入 IP 头，包括：

1. 原来是什么高层协议；
2. 发出包的主机名，主机名只要保证唯一性即可，使用数字或字符都可以，方便调试最好是字符串；
3. 报文序号，在每台主机上独立单增即可；
4. 在接收端根据主机名、报文序号去重，再恢复正常的 IP 头，使得过程透明；

## [20201214] 在 VirtaulBox 中配置一个具有一定结构的网络进行测试

- 资源：
  - [VirtualBox 的网卡配置](https://www.jianshu.com/p/0537b056790b)
  - [使用 netplan 模块预配置网络](https://netplan.io/reference/)

---

使用 VirtualBox 的图形界面最多为一台虚拟机配置 4 个网卡（使用配置文件可以配置 8 个，又不是界面放不下，这个设计有什么意义？）。因此为了方便配置为：

| 主机 | 内网名 | 网卡 | IP
| :-: | :-: | :-: | :-:
| VM0 | net0 | enp0s3  | 192.168.0.2/24
| VM0 | net1 | enp0s8  | 192.168.1.2/24
| VM1 | net0 | enp0s3  | 192.168.0.3/24
| VM1 | net1 | enp0s8  | 192.168.1.3/24
| VM1 | net2 | enp0s9  | 192.168.2.3/24
| VM1 | net3 | enp0s10 | 192.168.3.3/24
| VM2 | net2 | enp0s3  | 192.168.2.2/24
| VM2 | net3 | enp0s8  | 192.168.3.2/24

这个配置的意思是两台客户机 VM0 和 VM2 分别通过两条不同局域网中的链路连接到服务器 VM2。

以 VM1 的配置文件为例，`/etc/netplan/**.yaml`：

```yaml
network:
  ethernets:
    enp0s3:
      dhcp4: false
      addresses:
        - 192.168.0.3/24
    enp0s8:
      dhcp4: false
      addresses:
        - 192.168.1.3/24
    enp0s9:
      dhcp4: false
      addresses:
        - 192.168.2.3/24
    enp0s10:
      dhcp4: false
      addresses:
        - 192.168.3.3/24
```

## [20201222] 原来是这样

为了既**透明**又**不影响其他网络服务**，这个项目必须是一个完整的 VPN。通过这个应用连接的程序不再能了解网络的真实环境。

网络中所谓的 `转发服务器` 不是一个真正的服务器主机，而是一个虚拟网络的网络层转发设备，一个**虚拟路由器**。
在这个设计下，虚拟网络是一个夹在网络层和传输层之间的子层，对于真正的传输层来说它是网络层，
而它像一般的网络层对待链路层那样对待真正的网络层。

### 模块划分

1. 网卡

   网卡对象在程序中分为本地网卡和远程网卡。

   1. 本地网卡

      本地网卡用于发送真正的数据包，具有下列属性：

      - **在操作系统注册的网卡序号**
      - 网卡名字
      - 一组 IP 地址，只用其中固定的一个进行发送
      - 一个用于发送的网络层原始套接字

   2. 远程网卡

      远程网卡用于标识连接的对端，具有下列属性：

      - **远程主机的固定 IP**
      - **在操作系统注册的网卡序号**
      - 用于接收的 IP 地址

2. 连接

   连接是两个网卡之间的数据通道，具有下列属性：

   - **本地网卡对象的索引**
   - **远程网卡对象的索引**
   - 连接的带宽、延时等状态和性能指标

3. 聚合连接

   两个主机间所有有效连接的集合，控制着到用一个远程主机的一组连接。

### 工作流程

1. 启动网络上一个虚拟路由器。这个虚拟路由器具有 ARP 的功能，后期可以通过它连接虚拟的 DHCP 服务。
   虚拟路由器有线下商定好的公网 IP 和虚拟 IP，所有上线的主机启动时就已知这两个信息。

2. 启动虚拟主机。虚拟主机通过所有实际接口向路由器发送。

3. 路由器收到，开始建立连接。连接是双工的，可能需要同步？至少性能探测应该是双方同步的。

4. 双方维持连接存在，并且形成了两主机之间的连接束。

5. ... 交换路由表和其他功能，待续。

### 计划

1. 在两主机之间收发，发送时复制包、接收时去重。

2. 实际建立连接、探测性能。

3. 交换路由表、计算虚拟路由、转发。

4. 实现其他功能，如虚拟网络中的 DHCP。

### 数据结构

```c++
#include "net_device_t.h"

#include <unordered_map>
#include <unordered_set>

namespace autolabor::connection_aggregation
{
    /** 连接附加信息 */
    struct connection_info_t
    {
        // 此处应有同步状态、带宽、时延、丢包率等
    };

    struct program_t
    {
    private:
        // 本地网卡表
        // - 用网卡序号索引
        // - 来源：本地网络监测
        std::unordered_map<unsigned, net_device_t>
            _devices;

        // 远程网卡表
        // - 用远程主机地址和网卡序号索引
        // - 来源：远程数据包接收
        std::unordered_map<in_addr_t, std::unordered_map<unsigned, in_addr>>
            _remotes;

        // 连接表示法
        union connection_key_union
        {
            using key_t = uint64_t;
            key_t key;
            struct
            {
                uint32_t
                    src_index, // 本机网卡序号
                    dst_indet; // 远程主机网卡序号
            };
        };

        // 用一个整型作为索引以免手工实现 hash
        using connection_key_t = connection_key_union::key_t;

        // 连接信息用连接表示索引
        using connection_map_t = std::unordered_map<connection_key_t, connection_info_t>;

        // 连接表
        // - 用远程主机地址索引方便发送时构造连接束
        // - 来源：本地网卡表和远程网卡表的笛卡尔积
        std::unordered_map<in_addr_t, connection_map_t>
            _connection;
    };

} // namespace autolabor::connection_aggregation
```

## [20201223] 实现连接监视、设计协议

### 实现连接监视

通过 rtnetlink 的跟踪，在本机网卡发生变化时，可以实时调整连接，
从新的网卡建立新的连接，删除关闭或下线的网卡上的连接。

```c++
std::shared_mutex
  _local_mutex,      // 访问本机网卡表
  _remote_mutex,     // 访问远程网卡表
  _connection_mutex; // 访问连接表

// rtnetlink 套接字
fd_guard_t _netlink;
// 监视本地网络变化
void local_monitor();
// 发现新的地址或网卡
void address_added(unsigned, const char *, in_addr = {});
// 移除地址
void address_removed(unsigned, in_addr);
// 移除网卡
void device_removed(unsigned);
```

网络上数据包到达本机时，将调整远程网卡和连接。
或可由 api 写入固定的远程地址，适用于向虚拟路由器发起连接。
目前仅实现了 public method：

```c++
bool add_remote(in_addr, unsigned, in_addr);
```

### 程序状态描述

打印程序状态如下：

```bash
- tun device: user(20)
- local devices:
  - ens33(2): 192.168.18.185
- remote devices:
  - 10.0.0.2:
    - 4: 8.8.8.8
    - 2: 192.168.18.186
- connections:
  - 10.0.0.2:
    - 2 -> 4 : ...
    - 2 -> 2 : ...
```

分别是 tun 设备的名字和序号、已启动的本地网卡、已知的远程虚拟地址、网卡和实际地址、已建立的连接。
连接后面应该显示连接状态，但还没设计好。

### 设计收发函数和协议

包分为 3 类：

1. 基于单连接的包：
   1. 连接握手协议
   2. 用于性能探测的流量填充包
2. 基于连接束的内部包：
   1. 其他用于系统内部的协议包（如虚拟 DHCP、OSPF）
3. 来自应用层的数据包：
   1. 发送到本机应用的数据包
   2. 需要本机转发、发送到其他主机的数据包

3 种包都需要的附加信息：

1. 虚拟网络中的源地址
2. 连接描述符（本机网卡号+远程网卡号）

## [20201224] 实现包收发、设计转发协议

### ip 首部扩展

根据 ip 首部的设计精神，不变长的部分算首部。因此将程序收发的所有包都必带的部分放在首部里：

```c++
// 通用 ip 头附加信息
struct common_extra_t
{
    in_addr host;                  // 虚拟网络中的源地址
    uint32_t src_index, dst_index; // 本机网卡号、远程网卡号
};
```

这个扩充首部的长度是 12 字节，因此 ip 首部总长 20 + 12 = 32 字节。
ip 首部中的 id 用于传输一个在每个连接上分别计数的单增序号。

通过一个连接发送包的这个功能封装为一个私有函数：

```c++
// 发送单个包
size_t send_single(in_addr, connection_key_t, const iovec *, size_t);
```

参数分别是 `虚拟网络中的目的地址`、`连接描述符`、`要发送的分散负载`、`分散负载数量`。
其中 `虚拟网络中的目的地址` 和 `连接描述符` 唯一确定一个用于发送的实际套接字。

对于中继/虚拟路由器来说，应该先查路由表，此处填写的是下一跳的虚拟网络地址。
这就是所谓 `而它像一般的网络层对待链路层那样对待真正的网络层`（见 \[20201222\]）。

### 非通用附加信息

ip 首部之后的第一个字节确定其后还有什么添加的信息。对于一个转发包，结构是这样的：

```c++
struct forward_t
{
    uint8_t
        type,     // 附加信息标识符
        protocol; // 需要恢复到 ip 头的真实协议号
    uint16_t
        offset; // 需要恢复到 ip 头的真实段偏移
    in_addr
        src, // 虚拟网络中的源地址
        dst; // 虚拟网络中的目的地址
};
```

`type` 对于这个结构来说是个常数，暂定为 16。`protocol` 和 `offset` 直接来自被转发的 ip 首部。
`offset` 实际上只能是 0。发送端是否分段是可以控制的，没有必要分段，而如果在传输过程中发生重新分段，恢复是困难的。
首部中的源地址、目的地址填写了真实网络中的地址，因此需要在虚拟网络中路由的转发包，需要再记录 `src` 和 `dst`。

### 实现进度

今天实现了：
- 单连接上的收、发
- 接收时更新远程网卡记录和并新建连接
- 从 tun 描述符读取并修改首部后向网络转发
