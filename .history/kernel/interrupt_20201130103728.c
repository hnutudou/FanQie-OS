#include "interrupt.h"
#include "../lib/kernel/stdint.h"
#include "global.h"

#define IDT_COUNT 32

//中断门描述符
struct InterruptGate
{
    uint16_t    func_offset_low;    //中断处理程序低16位，低32位0-15
    uint16_t    func_offset_high;   //中断处理程序高16位
    uint16_t    selector;           //中断处理程序目标代码段选择子，低32位16-31
    uint8_t     fixed;              //高32位的0-8位，固定值
    uint8_t     attribute;          //高32位的8-16位
};
static struct InterruptGate idt[IDT_COUNT];

extern void *intr_entry_table[IDT_COUNT];

static void make_idt(struct InterruptGate *interrupt_gate, uint8_t attritube, intr_handler function)
{
    interrupt_gate->func_offset_low = (uint32_t)function & 0x0000ffff;
    interrupt_gate->func_offset_high = ((uint32_t)function >> 16) & 0x0000ffff;
    interrupt_gate->fixed = 0;
    interrupt_gate->attribute = attritube;
    interrupt_gate->selector = SELECTOR_K_CODE;
}

static void idt_desc_init()
{
    int i=0;
    for(; i<IDT_COUNT; ++i)
    {
        make_idt(&idt[i], IDT_DESC_ATTR_DPL0,  intr_entry_table[i]);
    }
    Puts("idt descriptor init done!\n");
}

void idt_init()
{
    Puts("idt init start!\n");
    idt_desc_init();
    pic_init();                                                                //初始化可编程中断控制器
    uint64_t idt_data =  (sizeof(idt) - 1) | (uint64_t)((uint32_t)idt << 16);  //32位表基址，16位表界限
}