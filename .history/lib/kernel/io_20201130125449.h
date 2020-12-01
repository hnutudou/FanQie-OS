#ifndef __FANQIE_IO_H__
#define __FANQIE_IO_H__
#include "stdint.h"

static inline void outb(uint16_t port, uint8_t data)
{
    asm("out %b0, %w1"::"a"(data), "Nd"(port));        //N:表示操作数为 0~255 之间的立即数
}

//向端口写，端口号存在dx中
static inline void outsw(uint16_t port, const void *addr, uint32_t count)
{
    asm volatile("cld; rep outsw":"+S"(addr),"+c"(count):"d"(port):"memory");     //S:表示寄存器 esi/si
}

static inline uint8_t readb(uint16_t port)
{
    uint8_t data;
    asm("in %w1, %b0" : "=a"(data) : "Nd"(port));
    return data;
}

//从端口中读count字节写入addr
static inline void insw(uint16_t port, const void *addr, uint32_t count)
{
    asm volatile("cld; rep insw":"+D"(addr),"+c"(count):"d"(port):"memory");
}
#endif