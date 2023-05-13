# ping実装例

## Echo request/reply フォーマット

```
Echo or Echo Reply Message

    0                   1                   2                   3
    0 1 2 3 4 5 6 7 8 9 0 1 2 3 4 5 6 7 8 9 0 1 2 3 4 5 6 7 8 9 0 1
   +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
   |     Type      |     Code      |          Checksum             |
   +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
   |           Identifier          |        Sequence Number        |
   +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
   |     Data ...
   +-+-+-+-+-
```

## Usage
```
Usage: myping [-c count] [-i interval] [-r] [-p] remote_host
       -c count
       -i interval (allow decimal number)
       -r: use raw socket  (socket(AF_INET, SOCK_RAW,   IPPROTO_ICMP)
       -p: use ping socket (socket(AF_INET, SOCK_DGRAM, IPPROTO_ICMP)
```

Linuxの場合、SOCK_RAWを使う場合
```
sudo setcap 'CAP_NET_RAW+eip' myping
```
とするとroot権限（あるいはsetuid root）なしに使うことができる。

iputilsのpingの例:
```
% getcap /usr/bin/ping
/usr/bin/ping = cap_net_admin,cap_net_raw+p
```

## socket(AF_INET SOCK_DGRAM, IPPROTO_ICMP) (ping socket) の使用

SOCK_DGRAM, IPPROTO_ICMPの使用は
sysctlで
```
net.ipv4.ping_group_range = 0   2147483647
```
の数字の間に所属するグループがicmp echo socket
(SOCK_DGRAM, IPPROTO_ICMPソケット)
を使うことができる(man 7 icmp)。

CentOS 7だと
net.ipv4.ping_group_range = 1   0

となっている。

ping socketを使用する場合recv()で読めるデータにIPヘッダは
入っていない。raw socketの場合はIPヘッダが入っている。

ping socketでは
```
   |           Identifier          |        Sequence Number        |
```
のIdentifierの部分はユーザーが書いても送られるデータでは
上書きされている（起動後、使われるたびに1づつインクリメントされる)。
