# ping実装例

## Usage
```
Usage: myping [-c count] [-i interval] [-r] [-p] remote_host
       -i interval (allow decimal number)
       -r: use raw socket  (socket(AF_INET, SOCK_RAW,   IPPROTO_ICMP)
       -p: use ping socket (socket(AF_INET, SOCK_DGRAM, IPPROTO_ICMP)
```

Linuxの場合、
```
sudo setcap 'CAP_NET_RAW+eip' myping
```
とするとroot権限（あるいはsetuid root）なしに使うことができる。

iputilsのpingの例:
```
% getcap /usr/bin/ping
/usr/bin/ping = cap_net_admin,cap_net_raw+p
```
