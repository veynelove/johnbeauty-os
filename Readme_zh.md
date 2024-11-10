### johnbeauty-os
this is a operating system by c++

# question

1 在gdt.cpp第10行，源代码是: 
    i[0]=(uint_32t)this;
    i[1]=sizeof(GlobalDescriptoTable)<<16;
    在调用interrupts.Activate()后，启动虚拟机失败，提示虚拟cpu异常。将i[0]和[1]交换后，正常收到硬件中断，不理解为什么。
    i[1] = (uint32_t)this;
    i[0] = sizeof(GlobalDescriptorTable) << 16;
