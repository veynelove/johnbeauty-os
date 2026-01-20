# johnbeauty-os

1. this is an operating system by c++.
2. 源项目地址:<https://www.youtube.com/playlist?list=PLHh55M_Kq4OApWScZyPl5HhgsTJS9MZ6M>
3. 学习搬运地址:<https://www.bilibili.com/video/BV1Ng411x7As?spm_id_from=333.788.videopod.episodes&vd_source=89f02ddd7438a1e3a8d5c5fa0a9ba297&p=8>
4. 源项目anthor: Viktor Engelmann (from Germany)
5. 源项目anthor个人主页：<http://www.algorithman.de/Autor/index.php>

---

## build

1. 使用"`make johnkernel.iso`"编译os镜像;
2. 使用 `make clean` 清理编译物;

### question

1. 在`gdt.cpp`第10行，源代码是:

    ``` cpp
    i[0]=(uint_32t)this;
    i[1]=sizeof(GlobalDescriptoTable)<<16;
    i[1] = (uint32_t)this;
    i[0] = sizeof(GlobalDescriptorTable) << 16;
    ```

    > 在调用interrupts.Activate()后，启动虚拟机失败，提示虚拟cpu异常。将i[0]和[1]交换后，正常收到硬件中断，不理解为什么。

2. 在`mouse.cpp`第60行,源代码是:

    ``` cpp
    for(uint8_t i=0;i<3;i++) {
            if((buffer[0] & (0x01<<i)) != (buttons & (0x01<<i))) {
                VideoMemory[80*y+x] = ((VideoMemory[80*y+x] & 0xF000)>>4)
                    | ((VideoMemory[80*y+x] & 0x0F00)<<4)
                    | ((VideoMemory[80*y+x] & 0x00FF));
            }
        }
    ```

    > 如果添加Video这段代码,在光标点击移动时,移动初始位置的颜色不会恢复.

### todo

1. 芯片驱动目前只有amd_am79c973,所以很多功能测试需要在amd芯片机器上跑；但大部分家庭电脑都是intel芯片；
2. 文件系统没有实现，附录课程最后一节的最后部分介绍了file system类的设计方案；

### tips

> 公主平安开心
