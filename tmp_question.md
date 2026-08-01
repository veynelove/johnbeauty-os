# qeustion

1. @内核栈可以任意设置大小吗，为什么4mb;随着内核体量增大，是否需要增加内核栈，增加衡量标准怎么衡量;
2. void jlos_hal_serial_default_init(void)串口初始化需要指定架构吧，现在是写死x86的配置吧;

3. icmp test: ping 192.168.159.144
4. tcp test: curl -v http://192.168.159.144:1234
5. udp test: echo "johnbeauty" | nc -u 192.168.159.144 5678
