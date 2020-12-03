#ifndef __FANQIE_MEMORY_H__
#define __FANQIE_MEMORY_H__

#include "stdint.h"
#include "../../kernel/assert.h"
#include "bitmap.h"
#include "print.h"

struct VirtualAddr
{
    struct Bitmap virtual_addr_bitmap;
    uint32_t virtual_addr_start;            //虚拟地址的起始值
};


//内存池
struct Pool
{
    struct Bitmap memory_pool_bitmap;   //位图
    uint32_t physical_addr_start;       //物理地址起始
    uint32_t memory_pool_size;          //内存容量
};
void memory_init();
#endif