/*
有缓存的广义文件 ByteIOContext 相关的文件操作，比如 open，read，close，seek 等等。
*/
#include "../berrno.h"
#include "avformat.h"
#include "avio.h"
#include <stdarg.h>

#define IO_BUFFER_SIZE 32768
//初始化广义文件 ByteIOContext 结构，一些简单的赋值操作。
int init_put_byte(ByteIOContext *s, 
				  unsigned char *buffer, 
				  int buffer_size, 
				  int write_flag, 
				  void *opaque, 
				  int(*read_buf)(void *opaque, uint8_t *buf, int buf_size), 
				  int(*write_buf)(void *opaque, uint8_t *buf, int buf_size), 
				  offset_t(*seek)(void *opaque, offset_t offset, int whence))
{
    s->buffer = buffer;
    s->buffer_size = buffer_size;
    s->buf_ptr = buffer;
    s->write_flag = write_flag;
    if (!s->write_flag)
        s->buf_end = buffer;
    else
        s->buf_end = buffer + buffer_size;
    s->opaque = opaque;
    s->write_buf = write_buf;
    s->read_buf = read_buf;
    s->seek = seek;
    s->pos = 0;
    s->must_flush = 0;
    s->eof_reached = 0;
    s->error = 0;
    s->max_packet_size = 0;

    return 0;
}

offset_t url_fseek(ByteIOContext *s, offset_t offset, int whence)
{
    offset_t offset1;
    //只支持 SEEK_CUR 和 SEEK_SET 定位方式，不支持 SEEK_END 方式。
    if (whence != SEEK_CUR && whence != SEEK_SET)
        return  - EINVAL;

    if (whence == SEEK_CUR)
    {
        //pos是buf_end 相对文件的偏移量
        //通过此语句算出buf_ptr的相对文件的偏移量
        offset1 = s->pos - (s->buf_end - s->buffer) + (s->buf_ptr - s->buffer);
        if (offset == 0)
            return offset1;
        offset += offset1;//seek之后的读写位置相对文件的偏移量
    }
    //通过此语句算出seek 之后的读写位置相对buffer的偏移量
    offset1 = offset - (s->pos - (s->buf_end - s->buffer));
    //可以在buffer区里seek
    if (offset1 >= 0 && offset1 <= (s->buf_end - s->buffer))
    {
        s->buf_ptr = s->buffer + offset1; // can do the seek inside the buffer
    }
    //不可以在buffer区里seek，则要通过文件seek操作从新设置buffer区域
    else
    {
        if (!s->seek)
            return  - EPIPE;
        //从新设置buffer
        s->buf_ptr = s->buffer;
        s->buf_end = s->buffer;
        //调用文件seek操作
        if (s->seek(s->opaque, offset, SEEK_SET) == (offset_t) - EPIPE)
            return  - EPIPE;
        s->pos = offset;
    }
    s->eof_reached = 0;

    return offset;
}
//广义文件 ByteIOContext 的当前实际偏移量再偏移 offset 字节，调用 url_fseek 实现
void url_fskip(ByteIOContext *s, offset_t offset)
{
    url_fseek(s, offset, SEEK_CUR);
}
//返回当前读写位置偏移量
offset_t url_ftell(ByteIOContext *s)
{
    return url_fseek(s, 0, SEEK_CUR);
}

offset_t url_fsize(ByteIOContext *s)
{
    offset_t size;

    if (!s->seek)
        return  - EPIPE;
    size = s->seek(s->opaque,  - 1, SEEK_END) + 1;
    s->seek(s->opaque, s->pos, SEEK_SET);
    return size;
}

int url_feof(ByteIOContext *s)
{
    return s->eof_reached;
}

int url_ferror(ByteIOContext *s)
{
    return s->error;
}

// Input stream
//从文件里读取数据填充到内部缓冲区
static void fill_buffer(ByteIOContext *s)
{
    int len;

    if (s->eof_reached)
        return ;
    //首先跳转的url_read_buf()函数，再跳转到url_read()，再跳转到实际文件协议的读函数完成读操作
    len = s->read_buf(s->opaque, s->buffer, s->buffer_size);
    if (len <= 0)
    {   // do not modify buffer if EOF reached so that a seek back can be done without rereading data
        s->eof_reached = 1;

        if (len < 0)
            s->error = len;
    }
    else
    {
        s->pos += len;
        s->buf_ptr = s->buffer;
        s->buf_end = s->buffer + len;
    }
}
//从内部缓冲区buf读取一字节数据
int get_byte(ByteIOContext *s) // NOTE: return 0 if EOF, so you cannot use it if EOF handling is necessary
{
    if (s->buf_ptr < s->buf_end)
    {
        return  *s->buf_ptr++;
    }
    else
    {
        fill_buffer(s);
        if (s->buf_ptr < s->buf_end)
            return  *s->buf_ptr++;
        else
            return 0;
    }
}
//从内部缓冲区buf读取2字节数据
unsigned int get_le16(ByteIOContext *s)
{
    unsigned int val;
    val = get_byte(s);
    val |= get_byte(s) << 8;
    return val;
}
//从内部缓冲区buf读取4字节数据
unsigned int get_le32(ByteIOContext *s)
{
    unsigned int val;
    val = get_le16(s);
    val |= get_le16(s) << 16;
    return val;
}

#define url_write_buf NULL
//把数据读到内部缓冲区buf里
static int url_read_buf(void *opaque, uint8_t *buf, int buf_size)
{
    URLContext *h = opaque;
    return url_read(h, buf, buf_size);
}
//实际调用url_seek
static offset_t url_seek_buf(void *opaque, offset_t offset, int whence)
{
    URLContext *h = opaque;
    return url_seek(h, offset, whence);
}
//设置缓冲区的大小,并重新分配空间
int url_setbufsize(ByteIOContext *s, int buf_size) // must be called before any I/O
{
    uint8_t *buffer;
    buffer = av_malloc(buf_size);
    if (!buffer)
        return  - ENOMEM;

    av_free(s->buffer);
    s->buffer = buffer;
    s->buffer_size = buf_size;
    s->buf_ptr = buffer;
    if (!s->write_flag)
        s->buf_end = buffer;
    else
        s->buf_end = buffer + buf_size;
    return 0;
}
//ByteIOContext层的打开文件函数
int url_fopen(ByteIOContext *s, const char *filename, int flags)
{
    URLContext *h;
	uint8_t *buffer;
    int buffer_size, max_packet_size;
    int err;
    //调用协议的open函数,这会初始化URLContext *h.
    err = url_open(&h, filename, flags);
    if (err < 0)
        return err;
   //获取一个包数据大小的最大值,如果为0则设置成IO_BUFFER_SIZE
    max_packet_size = url_get_max_packet_size(h);
    if (max_packet_size)
    {
        buffer_size = max_packet_size; // no need to bufferize more than one packet
    }
    else
    {
        buffer_size = IO_BUFFER_SIZE;
    }
    //分配内部缓冲区内存,大小为一个包数据大小的最大值
    buffer = av_malloc(buffer_size);
    if (!buffer)
	{
        url_close(h);
        return  - ENOMEM;
	}
    //初始化ByteIOContext s,  这里会把URLContext *h 关联到s里
    if (init_put_byte(s,
					  buffer, 
					  buffer_size, 
					  (h->flags & URL_WRONLY || h->flags & URL_RDWR), 
					  h, 
					  url_read_buf, 
					  url_write_buf, 
					  url_seek_buf) < 0)
    {
        url_close(h);
        av_free(buffer);
        return AVERROR_IO;
    }

    s->max_packet_size = max_packet_size;

    return 0;
}
//释放内部缓冲区buf,调用协议close函数
int url_fclose(ByteIOContext *s)
{
    URLContext *h = s->opaque;

    av_free(s->buffer);
    memset(s, 0, sizeof(ByteIOContext));
    return url_close(h);
}
//从广义缓冲区读取数据用于解码
int url_fread(ByteIOContext *s, unsigned char *buf, int size) // get_buffer
{
    int len, size1;

    size1 = size;
    while (size > 0)
    {
        len = s->buf_end - s->buf_ptr;
        if (len > size)//内部缓冲区可读数据大小足够
            len = size;
        if (len == 0)//内部缓冲区没有可读数据
        {
            //如果要读的数据大小size大于内部缓冲区数据的最大值,
            //则直接从文件里读取数据到输出,并且重置内部缓冲区buf
            if (size > s->buffer_size)
            {
                len = s->read_buf(s->opaque, buf, size);
                if (len <= 0)
                {
                    s->eof_reached = 1;
                    if (len < 0)
                        s->error = len;
                    break;
                }
                else
                {
                    s->pos += len;
                    size -= len;
                    buf += len;
                    s->buf_ptr = s->buffer;
                    s->buf_end = s->buffer /* + len*/;
                }
            }
            //如果要读的数据大小size小于内部缓冲区数据的最大值,
            //则先填充内部缓冲区
            else
            {
                fill_buffer(s);
                len = s->buf_end - s->buf_ptr;
                if (len == 0)
                    break;
            }
        }
        else//内部缓冲区有数据可读,则直接从内部缓冲区读取数据到输出
        {
            memcpy(buf, s->buf_ptr, len);
            buf += len;
            s->buf_ptr += len;
            size -= len;
        }
    }
    return size1 - size;
}
