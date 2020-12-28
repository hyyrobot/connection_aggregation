# 日志

## 目录

- [20201209 周三 对需求和技术的一些认识](#[20201209]-对需求和技术的一些认识)
- [20201210 周四 如何绑定网卡/确定实现方案](#[20201210]-如何绑定网卡/确定实现方案)
- [20201214 周一 在 VirtaulBox 中配置一个具有一定结构的网络进行测试](#[20201214]-在-VirtaulBox-中配置一个具有一定结构的网络进行测试)
  > 注意此处差了一周，这周写的代码已遗弃
- [20201222 周二 原来是这样](#[20201222]-原来是这样)
- [20201223 周三 实现连接监视、设计协议](#[20201223]-实现连接监视、设计协议)
- [20201224 周四 实现包收发、设计转发协议](#[20201224]-实现包收发、设计转发协议)
- [20201226 周六 使用 tun](#[20201226]-使用-tun)
- [20201227 周日 制造提供给 tun 的正确 udp 段](#[20201227]-制造提供给-tun-的正确-udp-段)
- [20201228 周一 实现对转发包去重（第一阶段完成）](#[20201228]-实现对转发包去重（第一阶段完成）)

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

ip 首部之后的第一个字节确定其后还有什么添加的信息。
文件 `src/protocol.h` 内容如下：

```c++
#include <netinet/in.h>

struct pack_type_t
{
    uint8_t state : 2; // 状态
    bool forward : 1;  // 转发包
    uint8_t zero : 5;  // 空
};

struct nothing_t
{
    uint8_t zero[4];
};

struct forward_t
{
    pack_type_t type; // 包类型和连接状态信息
    uint8_t protocol; // 需要恢复到 ip 头的真实协议号
    uint16_t offset;  // 需要恢复到 ip 头的真实段偏移
    in_addr
        src, // 虚拟网络中的源地址
        dst; // 虚拟网络中的目的地址
};
```

`pack_type_t` 中状态用于握手，转发包表示这个包的负载来自 tun。
握手不区分客户或服务器，规则是：

```c++
if (_state < 2)
{
    union
    {
        uint8_t byte;
        pack_type_t x;
    } convertor{.byte = type};
    _state = convertor.x.state + 1;
}
```

也就是说，状态包括 0、1、2，连接一端的状态是收到对方发的包中状态 + 1。
这个规则类似 TCP 但不严格，因为这个网络并不保证到达。各个状态的含义是：

| 状态 | 含义
| :-: | :-
| 0 | 我知道我能发出。
| 1 | 我知道我能接收，对端能发出。
| 2 | 我知道对端能接收，即，在这个状态双方都能收发。

转发包使用 `forward_t` 附加。其中，`protocol` 和 `offset` 直接来自被转发的 ip 首部。
`offset` 实际上只能是 0。发送端是否分段是可以控制的，没有必要分段，而如果在传输过程中发生重新分段，恢复是困难的。
首部中的源地址、目的地址填写了真实网络中的地址，因此需要在虚拟网络中路由的转发包，需要再记录 `src` 和 `dst`。

### 实现进度

今天实现了：
- 单连接上的收、发
- 接收时更新远程网卡记录和并新建连接
- 从 tun 描述符读取并修改首部后向网络转发
- 收发时记录连接状态和部分统计信息

## [20201226] 使用 tun

### 资料

之前对 tun 实验的太少，导致昨天卡了一天。新找到以下两篇有用的资料：

- [tun 接受写入什么](https://www.junmajinlong.com/virtual/network/all_about_tun_tap/)

  其中写道：
  
  > 用户空间的程序不可随意向虚拟网卡写入数据，因为写入虚拟网卡的这些数据都会被内核网络协议栈进行解封处理，就像来自物理网卡的数据都会被解封一样。
  >
  > 因此，如果用户空间程序要写 tun/tap 设备，所写入的数据需具有特殊结构：
  >
  > - 要么是已经封装了 PORT 的数据，即传输层的 tcp 数据段或 udp 数据报
  > - 要么是已经封装了 IP+PORT 的数据，即 ip 层数据包
  > - 要么是已经封装了 IP+PORT+MAC 的数据，即链路层数据帧
  > - 要么是其它符合 tcp/ip 协议栈的数据，比如二层的 PPP 点对点数据，比如三层的 icmp 协议数据
  
  因此 tun 对于用户程序来说纯粹就是个字符驱动设备，不会对用户的数组做任何修改，填写字段和校验和都需要用户手工进行，而且还不支持 `send`、`recv`。
  另外，亲测（而且显然）tun 并不支持不加 IP 头的报文。

- [初始化 tun 的高级配置（转载）](https://blog.csdn.net/bailyzheng/article/details/18770121)

  其中写道：

  > 为了使用TUN/TAP设备，我们必须包含特定的头文件 linux/if_tun.h，如 12 行所示。在 21 行，我们打开了字符设备 /dev/net/tun。
  > 接下来我们需要为 ioctl 的 TUNSETIFF 命令初始化一个结构体 ifr，一般的时候我们只需要关心其中的两个成员 ifr_name, ifr_flags。
  > ifr_name 定义了要创建或者是打开的虚拟网络设备的名字，如果它为空或者是此网络设备不存在，内核将新建一个虚拟网络设备，并 返回新建的虚拟网络设备的名字，同时文件描述符 fd 也将和此网络设备建立起关联。
  > 如果并没有指定网络设备的名字，内核将根据其类型自动选择 tunXX 和 tapXX 作为其名字。ifr_flags 用来描述网络设备的一些属性，比如说是点对点设备还是以太网设备。详细的选项解释如下:
  > 
  > - IFF_TUN: 创建一个点对点设备
  > - IFF_TAP: 创建一个以太网设备
  > - IFF_NO_PI: 不包含包信息，默认的每个数据包当传到用户空间时，都将包含一个附加的包头来保存包信息
  > - IFF_ONE_QUEUE: 采用单一队列模式，即当数据包队列满的时候，由虚拟网络设备自已丢弃以后的数据包直到数据包队列再有空闲。
  >
  > 配置的时候，IFF_TUN 和 IFF_TAP 必须择一，其他选项则可任意组合。其中 IFF_NO_PI 没有开启时所附加的包信息头如下:
  >
  > ```c++
  > struct tun_pi {
  >   unsigned short flags;
  >   unsigned short proto;
  > };
  > ```
  > 
  > 目前，flags 只在收取数据包的时候有效，当它的 TUN_PKT_STRIP 标志被置时，表示当前的用户空间缓冲区太小，以致数据包被截断。proto 成员表示发送/接收的数据包的协议。
  > 
  > 上面代码中的文件描述符 fd 除了支持 TUN_SETIFF 和其他的常规 ioctl 命令外，还支持以下命令:
  > 
  > - TUNSETNOCSUM: 不做校验和校验。参数为 int 型的 bool 值。
  > - TUNSETPERSIST: 把对应网络设备设置成持续模式，默认的虚拟网络设备，当其相关的文件符被关闭时，也将会伴随着与之相关的路由等信息同时消失。如果设置成持续模式，那么它将会被保留供以后使用。参数为 int 型的 bool 值。
  > - TUNSETOWNER: 设置网络设备的属主。参数类型为 uid_t。
  > - TUNSETLINK: 设置网络设备的链路类型，此命令只有在虚拟网络设备关闭的情况下有效。参数为 int 型。
  
  以后强了可以不检验校验和。

### 示例程序

下面一段程序用于实现一个会回应 ping 的 tun：

```c++
uint8_t buffer[128]{};
while (true)
{
    // 从 tun 读
    auto n = read(tun, buffer, sizeof(buffer));
    auto header = reinterpret_cast<ip *>(buffer);
    // 只响应 ICMP
    if (header->ip_p != IPPROTO_ICMP)
        continue;
    // 处理 IP 报头
    // offset 字段只能是 0 或 64
    // length 字段只能是网络字节序
    // id 字段是任意的
    // 字段的顺序不影响校验和
    header->ip_id = 999;
    fill_checksum(header);
    // 处理 ICMP 报头
    auto icmp_ = reinterpret_cast<icmp *>(buffer + sizeof(ip));
    // 交换 IP 头中的地址，不影响 IP 校验和
    std::swap(header->ip_src, header->ip_dst);
    // 类型变为响应
    icmp_->icmp_type = ICMP_ECHOREPLY;
    // ICMP 包长度是 8 报头 + 56 报文
    icmp_->icmp_cksum = 0;
    icmp_->icmp_cksum = check_sum(icmp_, n - sizeof(ip));
    // 写回 tun
    std::cout << *header << std::endl;
    write(tun, buffer, n);
}
```

其中使用到计算校验和函数：

```c++
uint16_t checksum(const void *data, size_t n)
{
    auto p = reinterpret_cast<const uint16_t *>(data);
    uint32_t sum = 0;

    for (; n > 1; n -= 2)
        sum += *p++;

    if (n)
        sum += *p;

    p = reinterpret_cast<uint16_t *>(&sum);
    sum = p[0] + p[1];
    sum = p[0] + p[1];

    return ~p[0];
}

void fill_checksum_ip(ip *data)
{
    data->ip_sum = checksum(data, sizeof(ip));
}
```

## [20201227] 制造提供给 tun 的正确 udp 段

简单来说，所有 len 都是网络字节序。

下面一段程序直接制造一个将字符串 `text` 封装到网络层的 udp 段并写入 tun：

```c++
uint8_t buffer[128];
auto ip_ = reinterpret_cast<ip *>(buffer);
auto udp_ip_ = reinterpret_cast<udp_ip_t *>(buffer + udp_ip_offset);
auto udp_ = reinterpret_cast<udphdr *>(buffer + sizeof(ip));
auto data = reinterpret_cast<char *>(buffer + sizeof(ip) + sizeof(udphdr));

std::strcpy(data, text);
auto udp_len = sizeof(udphdr) + std::strlen(text);
// 填充 udp 首部
udp_->source = 0;           // 源端口，可以为 0
udp_->dest = 9999;          // 目的端口
udp_->len = htons(udp_len); // 段长度 = 网络字节序(udp 头长度 + 数据长度)
udp_->check = 0;            // 清零校验和段
// 填充 udp 伪首部
*udp_ip_ = {
    .length = udp_->len,     // 再来一遍长度
    .protocol = IPPROTO_UDP, // udp 协议号
    .src = local,            // 源 ip 地址
    .dst = address,          // 目的 ip 地址
};
fill_checksum_udp(udp_ip_);

*ip_ = {
    .ip_hl = 5,
    .ip_v = 4,
    .ip_len = htons(sizeof(ip) + udp_len),
    .ip_ttl = 64,
    .ip_p = IPPROTO_UDP,
    .ip_src = local,
    .ip_dst = address,
};
fill_checksum_ip(ip_);

while (true)
{
    write(tun, buffer, sizeof(ip) + udp_len);
    std::this_thread::sleep_for(1s);
}
```

其中用到 udp 伪首部类型定义和填充 udp 校验和的函数：

```c++
struct udp_ip_t
{
    uint16_t length;
    uint8_t zero, protocol;
    in_addr src, dst;
};

constexpr size_t udp_ip_offset = sizeof(ip) - sizeof(udp_ip_t);

void fill_checksum_udp(udp_ip_t *data)
{
    reinterpret_cast<udphdr *>(data + 1)->check = checksum(data, sizeof(udp_ip_t) + ntohs(data->length));
}
```

此时监听 `address:9999` 就能收到 `text` 了。

## [20201228] 实现对转发包去重（第一阶段完成）

由于 UDP 包校验码和 TCP 包还有关（什么鬼设计！），为了避免麻烦，改为使用 IP 协议号 4（IPinIP），直接再套一个 IP 头来实现。

因此也修改了包附加信息：

```c++
struct extra_t
{
    uint8_t state : 2; // 状态
    bool forward : 1;  // 转发包
    uint16_t id : 13;  // 基于连接束的序号
};
```

这个结构体总共占 2 字节，其中 2 位用于握手，1 位用于标记是否要向传输层转发，13 位用于对广播包去重。

连接束类（`class connection_srand_t`）中通过维护一些数据结构，检查每个 id 上一次接收到的时间，
如果距离上一次接收超过 500ms 则认为两次不是同一包，实现去重。
