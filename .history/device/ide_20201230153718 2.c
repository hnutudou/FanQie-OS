#include "ide.h"
#define BIT_STAT_BSY    0x80 // 硬盘忙
#define BIT_STAT_DRDY   0x40 // 驱动器准备好
#define BIT_STAT_DRQ    0x8  // 数据传输准备好了

/* device 寄存器的一些关键位 */ 
#define BIT_DEV_MBS 0xa0 
#define BIT_DEV_LBA 0x40 
#define BIT_DEV_DEV 0x10
#define reg_data(channel)	 (channel->port_start + 0)
#define reg_error(channel)	 (channel->port_start + 1)
#define reg_sect_cnt(channel)	 (channel->port_start + 2)
#define reg_lba_l(channel)	 (channel->port_start + 3)
#define reg_lba_m(channel)	 (channel->port_start + 4)
#define reg_lba_h(channel)	 (channel->port_start + 5)
#define reg_dev(channel)	 (channel->port_start + 6)
#define reg_status(channel)	 (channel->port_start + 7)
#define reg_cmd(channel)	 (reg_status(channel))
#define reg_alt_status(channel)  (channel->port_start + 0x206)
#define reg_ctl(channel)	 reg_alt_status(channel)
/* 一些硬盘操作的指令 */ 
#define CMD_IDENTIFY        0xec // identify指令,获取硬盘的身份信息
#define CMD_READ_SECTOR     0x20 // 读扇区指令 
#define CMD_WRITE_SECTOR    0x30 // 写扇区指令
  
uint8_t channel_cnt; // 按硬盘数计算的通道数 
struct Channel channels[2]; // 有两个 ide 通道


uint32_t lba_start = 0;             //总扩展分区的起始lba
uint8_t p_no = 0, l_no = 0;         //硬盘主分区和逻辑分区的下标
struct List partition_list;         //分区链表
struct PartitionTable
{
    uint8_t bootable;       // 是否可引导
    uint8_t start_head;     // 起始磁头号
    uint8_t start_sec;      // 起始扇区号
    uint8_t start_chs;      //起始柱面号
    uint8_t fs_type;        //分区类型
    uint8_t end_head;       //结束磁头号
    uint8_t end_sec;        //结束扇区号
    uint8_t end_chs;        //结束柱面号
    uint32_t start_lba;     //起始lba地址
    uint32_t sec_cnt;       //本分区的扇区数
}__attribute__ ((packed));

//引导扇区
struct BootSector
{
    uint8_t boot[446];              //引导代码
    struct PartitionTable partition_table[4]; //分区表中有4项，16*4
    uint16_t magic;                 //魔数0x55 0xaa

}__attribute__ ((packed));          //不允许编译器为对齐而在此结构中填充空隙



void select_disk(struct Disk *hd)
{
     uint8_t reg_device = BIT_DEV_MBS | BIT_DEV_LBA;
   if (hd->device_no == 1) {	// 若是从盘就置DEV位为1
      reg_device |= BIT_DEV_DEV;
   }
   outb(reg_dev(hd->channel), reg_device);
}
//向硬盘控制器写入起始扇区地址以及要读的扇区数
void select_sector(struct Disk* hd, uint32_t lba, uint32_t size)
{
     struct Channel* channel = hd->channel;

   /* 写入要读写的扇区数*/
   outb(reg_sect_cnt(channel), size);	 // 如果sec_cnt为0,则表示写入256个扇区

   /* 写入lba地址(即扇区号) */
   outb(reg_lba_l(channel), lba);		 // lba地址的低8位,不用单独取出低8位.outb函数中的汇编指令outb %b0, %w1会只用al。
   outb(reg_lba_m(channel), lba >> 8);		 // lba地址的8~15位
   outb(reg_lba_h(channel), lba >> 16);		 // lba地址的16~23位

   /* 因为lba地址的24~27位要存储在device寄存器的0～3位,
    * 无法单独写入这4位,所以在此处把device寄存器再重新写入一次*/
   outb(reg_dev(channel), BIT_DEV_MBS | BIT_DEV_LBA | (hd->device_no == 1 ? BIT_DEV_DEV : 0) | lba >> 24);
}

//发命令
void cmd_out(struct Channel* channel, uint8_t cmd)
{
   channel->in_intr = 1;
   outb(reg_cmd(channel), cmd);
}

//读size个扇区到buf
void read_sector(struct Disk *disk, void *buf, uint32_t size)
{
    uint32_t size_in_byte;
   if (size == 0) {
   /* 因为sec_cnt是8位变量,由主调函数将其赋值时,若为256则会将最高位的1丢掉变为0 */
      size_in_byte = 256 * 512;
   } else { 
      size_in_byte = size * 512; 
   }
   insw(reg_data(disk->channel), buf, size_in_byte / 2);
}
/*
//写buf中的size个扇区到硬盘
void write_sector(struct Disk *disk, void *buf, uint32_t size)
{
    size = size * 512;
    outsw(disk->channel->port_start, buf, size/2);
}
*/
/*
uint8_t busy_wait(struct Disk *disk)
{
    uint32_t temp_time = 30*1000;
    while(temp_time > 0)
    {
        if (!(readb(disk->channel->port_start+7) & BIT_STAT_BSY)) 
        { 
            return (readb(disk->channel->port_start+7) & BIT_STAT_DRQ);
        } 
        else 
        {
            ticks_sleep(10);
        }
        temp_time -= 10;
    }
    return 0;
}
*/

//从硬盘读size个扇区到buf
void read_disk(struct Disk *disk, uint32_t lba, void *buf, uint32_t size)
{
    lock_acquire(&disk->channel->lock);
    select_disk(disk);                      //选择操作的硬盘
    uint32_t sec_do, sec_done = 0;
    while(sec_done < size)
    {
        //每次操作的扇区数
        if(sec_done + 256 <= size)   sec_do = 256;
        else                        sec_do = size - sec_done;

        select_sector(disk, lba+sec_done, sec_do);         //写入待读入的扇区数和起始扇区号
        cmd_out(disk->channel, CMD_READ_SECTOR);    //准备读数据
        
        sem_wait(&disk->channel->disk_done);        //阻塞
        // if(!busy_wait(disk))                        //失败
        // {
        //     Puts(disk->name);
        //     Puts(" read sector ");
        //     Putint(lba);
        //     Puts(" failed\n");
        //     return ;
        // }
        read_sector(disk, (void *)((uint32_t)buf+sec_done*512), sec_do);
        sec_done += sec_do;

    }
    lock_release(&disk->channel->lock);    
}

//写size个扇区到硬盘
void write_disk(struct Disk *disk, uint32_t lba, void *buf, uint32_t size)
{
    lock_acquire(&disk->channel->lock);
    select_disk(disk);                      //选择操作的硬盘
    uint32_t sec_do, sec_done = 0;
    //8位寄存器最多读255个
    while(sec_done < size)
    {
        //每次操作的扇区数
        if(sec_done + 256 < size)   sec_do = 256;
        else                        sec_do = size - sec_done;

        select_sector(disk, lba, size);         //写入待读入的扇区数和起始扇区号
        cmd_out(disk->channel, CMD_WRITE_SECTOR);    //准备读数据
        
        /*
        if(!busy_wait(disk))                        //失败
        {
            Puts(disk->name);
            Puts(" read sector ");
            Putint(lba);
            Puts(" failed\n");
            return ;
        }
        */
        write_sector(disk, (void *)((uint32_t)buf+sec_done*512), sec_do);
        sem_wait(&disk->channel->disk_done);        //阻塞，参数更新需要互斥
        sec_done += sec_do;

    }
    lock_release(&disk->channel->lock);
}

void intr_disk_handler(void* intr_no)
{
    uint8_t no = *(uint8_t *)intr_no - 0x2e;
    struct Channel *channel = &channels[no];
    Puts("this is first out\n");
    if(channel->in_intr)
    {
        channel->in_intr = 0;
        Puts("this is out\n");
        sem_post(&channel->disk_done);       //解开阻塞，硬盘完成操作后会发中断信号
        readb(channel->port_start + 7);
    }
}


void swap_bytes(const char *x, char *buf, uint32_t len)
{
    uint8_t index;
    for(index = 0; index < len; index+=2)
    {
        buf[index + 1] = *x;
        x++;
        buf[index]   = *x;
        x++;
    }
    buf[index] = '\0';
}


//获取硬盘信息
void identify_disk(struct Disk *hd)
{
    char id_info[512];
   select_disk(hd);
   cmd_out(hd->channel, CMD_IDENTIFY);
/* 向硬盘发送指令后便通过信号量阻塞自己,
 * 待硬盘处理完成后,通过中断处理程序将自己唤醒 */
   sem_wait(&hd->channel->disk_done);

/* 醒来后开始执行下面代码*/
   if (!busy_wait(hd)) {     //  若失败
      char error[64];
      sprintf(error, "%s identify failed!!!!!!\n", hd->name);
      //PANIC(error);
   }
   read_sector(hd, id_info, 1);

   char buf[64];
   uint8_t sn_start = 10 * 2, sn_len = 20, md_start = 27 * 2, md_len = 40;
   swap_bytes(&id_info[sn_start], buf, sn_len);
  // printk("   disk %s info:\n      SN: %s\n", hd->name, buf);
   memset(buf, 0, sizeof(buf));
   swap_bytes(&id_info[md_start], buf, md_len);
  // printk("      MODULE: %s\n", buf);
   uint32_t sectors = *(uint32_t*)&id_info[60 * 2];
  // printk("      SECTORS: %d\n", sectors);
  // printk("      CAPACITY: %dMB\n", sectors * 512 / 1024 / 1024);
}


void partitioan_scan(struct Disk *disk, uint32_t lba)
{
    struct BootSector* boot_sector = sys_malloc(sizeof(struct BootSector));
    read_disk(disk, lba, boot_sector, 1);
    uint8_t index = 0;
    struct PartitionTable *table = boot_sector->partition_table;
    //遍历四个分区表
    while(index++ < 4)
    {
        if(table->fs_type == 0x5)
        {
            //扩展分区
            if(lba_start != 0)
            {
                partitioan_scan(disk, table->start_lba+lba_start);
            }
            else
            {
                //第一次读取引导快
                lba_start = table->start_lba;
                partitioan_scan(disk, table->start_lba);   
            }
        }
        else if (table->fs_type != 0)
        {
            //有效的分区类型
            if(lba == 0)
            {
                disk->prim_parts[p_no].start_lba = lba_start + table->start_lba;
                disk->prim_parts[p_no].section_num = table->sec_cnt;
                disk->prim_parts[p_no].disk = disk;
                list_push_back(&partition_list, &disk->prim_parts[p_no].part_tag);
                sprintf(disk->prim_parts[p_no].name, "%s%d", disk->name, p_no+1);
                p_no++;
            }
            else
            {
                disk->prim_parts[l_no].start_lba = lba_start + table->start_lba;
                disk->prim_parts[l_no].section_num = table->sec_cnt;
                disk->prim_parts[l_no].disk = disk;
                list_push_back(&partition_list, &disk->prim_parts[l_no].part_tag);
                sprintf(disk->prim_parts[l_no].name, "%s%d", disk->name, l_no+5);
                l_no++;
                if(l_no >= 8)    return ;
            }
            
        }
        table++;
    }
    sys_free(boot_sector);
}

bool info(struct ListPtr *list, int arg )
{
    struct Partition pa;
    uint32_t len =  (uint32_t)&pa.part_tag - (uint32_t)&pa;
    struct Partition *part = (struct Partition *)((uint32_t)list - len);
    Puts("      "); Puts(part->name); Puts(" start_lba:0x");
    Putint(part->start_lba); Puts(", sec_cnt:0x"); Putint(part->section_num);
    Puts("\n");
    return false;
}




void ide_init()
{
  

    Puts("ide_init start\n");
   uint8_t hd_cnt = *((uint8_t*)(0x475));	      // 获取硬盘的数量
   ASSERT(hd_cnt > 0);
   list_init(&partition_list);
   channel_cnt = (hd_cnt + hd_cnt - 1) / 2 ;	   // 一个ide通道上有两个硬盘,根据硬盘数量反推有几个ide通道
   struct Channel* channel;
   uint8_t channel_no = 0, dev_no = 0; 

   /* 处理每个通道上的硬盘 */
   while (channel_no < channel_cnt) {
      channel = &channels[channel_no];
      sprintf(channel->name, "ide%d", channel_no);

      /* 为每个ide通道初始化端口基址及中断向量 */
      switch (channel_no) {
	 case 0:
	    channel->port_start	 = 0x1f0;	   // ide0通道的起始端口号是0x1f0
	    channel->intr_num	 = 0x20 + 14;	   // 从片8259a上倒数第二的中断引脚,温盘,也就是ide0通道的的中断向量号
	    break;
	 case 1:
	    channel->port_start	 = 0x170;	   // ide1通道的起始端口号是0x170
	    channel->intr_num	 = 0x20 + 15;	   // 从8259A上的最后一个中断引脚,我们用来响应ide1通道上的硬盘中断
	    break;
      }

      channel->in_intr = false;		   // 未向硬盘写入指令时不期待硬盘的中断
      lock_init(&channel->lock);		     

   /* 初始化为0,目的是向硬盘控制器请求数据后,硬盘驱动sema_down此信号量会阻塞线程,
   直到硬盘完成后通过发中断,由中断处理程序将此信号量sema_up,唤醒线程. */
      sem_init(&channel->disk_done, 0);

      register_intr_handler(channel->intr_num, intr_disk_handler);

      /* 分别获取两个硬盘的参数及分区信息 */
      while (dev_no < 2) {
	 struct Disk* hd = &channel->devices[dev_no];
	 hd->channel = channel;
	 hd->device_no = dev_no;
	 sprintf(hd->name, "sd%c", 'a' + channel_no * 2 + dev_no);
	 identify_disk(hd);	 // 获取硬盘参数
	 if (dev_no != 0) {	 // 内核本身的裸硬盘(hd60M.img)不处理
	    partitioan_scan(hd, 0);  // 扫描该硬盘上的分区  
	 }
	 p_no = 0, l_no = 0;
	 dev_no++; 
      }
      dev_no = 0;			  	   // 将硬盘驱动器号置0,为下一个channel的两个硬盘初始化。
      channel_no++;				   // 下一个channel
   }

}