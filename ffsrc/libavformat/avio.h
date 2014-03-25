/*
文件读写模块定义的数据结构和函数声明，ffplay 把这些全部放到这个.h 文件中
*/
#ifndef AVIO_H
#define AVIO_H

#define URL_EOF (-1)

typedef int64_t offset_t;

#define URL_RDONLY 0
#define URL_WRONLY 1
#define URL_RDWR   2
//ByteIOContext的opaque 会连接此结构体
typedef struct URLContext
{
    struct URLProtocol *prot;//本例指向file_protocol
    int flags;
    int max_packet_size; // if non zero, the stream is packetized with this max packet size
    void *priv_data;//关联读写协议的某些有用的参数，本例指向文件描述符
    char filename[1]; // specified filename
} URLContext;
//相当于一个借口，定义各种读写协议要提供的功能
typedef struct URLProtocol
{
    const char *name;
    int(*url_open)(URLContext *h, const char *filename, int flags);
    int(*url_read)(URLContext *h, unsigned char *buf, int size);
    int(*url_write)(URLContext *h, unsigned char *buf, int size);
    offset_t(*url_seek)(URLContext *h, offset_t pos, int whence);
    int(*url_close)(URLContext *h);
    struct URLProtocol *next;
} URLProtocol;

typedef struct ByteIOContext
{
    unsigned char *buffer;//缓存首地址
    int buffer_size;//缓存大小
    unsigned char *buf_ptr,  *buf_end;//缓存读写指针以及结束指针
    void *opaque;//关联URLContext
    int (*read_buf)(void *opaque, uint8_t *buf, int buf_size);
    int (*write_buf)(void *opaque, uint8_t *buf, int buf_size);
    offset_t(*seek)(void *opaque, offset_t offset, int whence);
    offset_t pos;    // position in the file of the current buffer
    int must_flush;  // true if the next seek should flush
    int eof_reached; // true if eof reached
    int write_flag;  // true if open for writing
    int max_packet_size;
    int error;       // contains the error code or 0 if no error happened
} ByteIOContext;

int url_open(URLContext **h, const char *filename, int flags);
int url_read(URLContext *h, unsigned char *buf, int size);
int url_write(URLContext *h, unsigned char *buf, int size);
offset_t url_seek(URLContext *h, offset_t pos, int whence);
int url_close(URLContext *h);
int url_get_max_packet_size(URLContext *h);

int register_protocol(URLProtocol *protocol);

int init_put_byte(ByteIOContext *s, 
				  unsigned char *buffer, 
				  int buffer_size, 
				  int write_flag, 
				  void *opaque, 
				  int(*read_buf)(void *opaque, uint8_t *buf, int buf_size),
				  int(*write_buf)(void *opaque, uint8_t *buf, int buf_size), 
				  offset_t(*seek)(void *opaque, offset_t offset, int whence));

offset_t url_fseek(ByteIOContext *s, offset_t offset, int whence);
void url_fskip(ByteIOContext *s, offset_t offset);
offset_t url_ftell(ByteIOContext *s);
offset_t url_fsize(ByteIOContext *s);
int url_feof(ByteIOContext *s);
int url_ferror(ByteIOContext *s);

int url_fread(ByteIOContext *s, unsigned char *buf, int size); // get_buffer
int get_byte(ByteIOContext *s);
unsigned int get_le32(ByteIOContext *s);
unsigned int get_le16(ByteIOContext *s);

int url_setbufsize(ByteIOContext *s, int buf_size);
int url_fopen(ByteIOContext *s, const char *filename, int flags);
int url_fclose(ByteIOContext *s);

int url_open_buf(ByteIOContext *s, uint8_t *buf, int buf_size, int flags);
int url_close_buf(ByteIOContext *s);

#endif
