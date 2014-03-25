/*
此文件实现了 URLProtocol 抽象层广义文件操作函数，由于 URLProtocol 是底层其他具体文件 (file,pipe 等)
的简单封装，这一层只是一个中转站，大部分函数都是简单中转到底层的具体实现函数。
*/
#include "../berrno.h"
#include "avformat.h"

URLProtocol *first_protocol = NULL;

int register_protocol(URLProtocol *protocol)
{
    URLProtocol **p;
    p = &first_protocol;
    while (*p != NULL)
        p = &(*p)->next;
    *p = protocol;
    protocol->next = NULL;
    return 0;
}
//获取url 的前缀协议名称，然后从协议队列里寻找一个名称匹配的协议，
//根据去协议设置URLContext来关联该协议，最后调用协议的open函数
int url_open(URLContext **puc, const char *filename, int flags)
{
    URLContext *uc;
    URLProtocol *up;
    const char *p;
    char proto_str[128],  *q;
    int err;

    p = filename;
    q = proto_str;
    while (*p != '\0' &&  *p != ':')//根据url前缀获取协议名称
    {
        if (!isalpha(*p))  // protocols can only contain alphabetic chars
            goto file_proto;
        if ((q - proto_str) < sizeof(proto_str) - 1)
            *q++ =  *p;
        p++;
    }
    // if the protocol has length 1, we consider it is a dos drive
    if (*p == '\0' || (q - proto_str) <= 1)
    {
file_proto: 
		strcpy(proto_str, "file");
    }
    else
    {
        *q = '\0';
    }
    //从协议队列里寻找名称匹配的协议实例
    up = first_protocol;
    while (up != NULL)
    {
        if (!strcmp(proto_str, up->name))
            goto found;
        up = up->next;
    }
    err =  - ENOENT;
    goto fail;
found: 
	uc = av_malloc(sizeof(URLContext) + strlen(filename));
    if (!uc)
    {
        err =  - ENOMEM;
        goto fail;
    }
    strcpy(uc->filename, filename);
    uc->prot = up;//把协议保存在URLContext的port成员里
    uc->flags = flags;
    uc->max_packet_size = 0; // default: stream file
    err = up->url_open(uc, filename, flags);
    if (err < 0)
    {
        av_free(uc);
        *puc = NULL;
        return err;
    }
    *puc = uc;
    return 0;
fail:
	*puc = NULL;
    return err;
}

int url_read(URLContext *h, unsigned char *buf, int size)
{
    int ret;
    if (h->flags &URL_WRONLY)
        return AVERROR_IO;
    ret = h->prot->url_read(h, buf, size);
    return ret;
}

offset_t url_seek(URLContext *h, offset_t pos, int whence)
{
    offset_t ret;

    if (!h->prot->url_seek)
        return  - EPIPE;
    ret = h->prot->url_seek(h, pos, whence);
    return ret;
}

int url_close(URLContext *h)
{
    int ret;

    ret = h->prot->url_close(h);
    av_free(h);
    return ret;
}

int url_get_max_packet_size(URLContext *h)
{
    return h->max_packet_size;
}
