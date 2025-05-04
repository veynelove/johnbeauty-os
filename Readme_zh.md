### johnbeauty-os
this is a operating system by c++.
源项目地址:https://www.bilibili.com/video/BV1Ng411x7As?spm_id_from=333.788.videopod.episodes&vd_source=89f02ddd7438a1e3a8d5c5fa0a9ba297&p=8

# build
1 使用"make johnkernel.iso"编译os镜像.

# question

1 在gdt.cpp第10行，源代码是: 
    i[0]=(uint_32t)this;
    i[1]=sizeof(GlobalDescriptoTable)<<16;
    在调用interrupts.Activate()后，启动虚拟机失败，提示虚拟cpu异常。将i[0]和[1]交换后，正常收到硬件中断，不理解为什么。
    i[1] = (uint32_t)this;
    i[0] = sizeof(GlobalDescriptorTable) << 16;

2 在mouse.cpp第60行,源代码是:
    for(uint8_t i=0;i<3;i++) {
            if((buffer[0] & (0x01<<i)) != (buttons & (0x01<<i))) {
                VideoMemory[80*y+x] = ((VideoMemory[80*y+x] & 0xF000)>>4)
                    | ((VideoMemory[80*y+x] & 0x0F00)<<4)
                    | ((VideoMemory[80*y+x] & 0x00FF));
            }
        }
    如果添加Video这段代码,在光标点击移动时,移动初始位置的颜色不会恢复.
