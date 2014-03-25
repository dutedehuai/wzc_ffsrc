#include "avformat.h"

extern URLProtocol file_protocol;

void av_register_all(void)
{
    static int inited = 0;

    if (inited != 0)
        return ;
    inited = 1;
    //初始化CPU 相关加速指令
    avcodec_init();
    //把各种解码器添加到头部为first_avcodec的链表里
    avcodec_register_all();
    //把各种文件格式添加到头部为first_iformat的链表里
    avidec_init();
    //把各种文件读写协议添加到头部为first_protocol的链表里
    register_protocol(&file_protocol);
}
