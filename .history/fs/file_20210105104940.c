#include "file.h"
#define MAX_FILE_NUM 32

struct File file_table[MAX_FILE_NUM];

int get_slot()
{
    //标准输入、输出、错误---0，1，2
    uint32_t index = 3;
    for(; index<MAX_FILE_NUM; ++index)
    {
        if(file_table[index].inode == (void*)0)
        {
            break;
        }
    }
    if(index == MAX_FILE_NUM)
    {
        return -1;
    }
    return index;
}

int install_pcb(uint32_t index)
{
    struct TaskStruct *task = running_thread();
    uint32_t index = 3;
    for(; index<8; ++index)
    {
        if(file_table[index].inode == (void*)0)
        {
            break;
        }
    }
}