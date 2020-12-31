#include "ide.h"
#define BIT_ALT_STAT_BSY    0x80 // 硬盘忙
#define BIT_ALT_STAT_DRDY   0x40 // 驱动器准备好
#define BIT_ALT_STAT_DRQ    0x8  // 数据传输准备好了

/* device 寄存器的一些关键位 */ 
#define BIT_DEV_MBS 0xa0 
#define BIT_DEV_LBA 0x40 
#define BIT_DEV_DEV 0x10

/* 一些硬盘操作的指令 */ 
#define CMD_IDENTIFY        0xec // identify指令
#define CMD_READ_SECTOR     0x20 // 读扇区指令 
#define CMD_WRITE_SECTOR    0x30 // 写扇区指令
  
uint8_t channel_cnt; // 按硬盘数计算的通道数 
struct Channel channels[2]; // 有两个 ide 通道