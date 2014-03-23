#include "../berrno.h"
#include "avformat.h"
#include <assert.h>

#define UINT_MAX  (0xffffffff)

#define PROBE_BUF_MIN 2048
#define PROBE_BUF_MAX 131072

AVInputFormat *first_iformat = NULL;

void av_register_input_format(AVInputFormat *format)
{
    AVInputFormat **p;
    p = &first_iformat;
    while (*p != NULL)
        p = &(*p)->next;
    *p = format;
    format->next = NULL;
}

int match_ext(const char *filename, const char *extensions)
{
    const char *ext,  *p;
    char ext1[32],  *q;

    if (!filename)
        return 0;

    ext = strrchr(filename, '.');
    if (ext)
    {
        ext++;
        p = extensions;
        for (;;)
        {
            q = ext1;
            while (*p != '\0' &&  *p != ',' && q - ext1 < sizeof(ext1) - 1)
                *q++ =  *p++;
            *q = '\0';
            if (!strcasecmp(ext1, ext))
                return 1;
            if (*p == '\0')
                break;
            p++;
        }
    }
    return 0;
}

AVInputFormat *av_probe_input_format(AVProbeData *pd, int is_opened)
{
    AVInputFormat *fmt1,  *fmt;
    int score, score_max;

    fmt = NULL;
    score_max = 0;
    for (fmt1 = first_iformat; fmt1 != NULL; fmt1 = fmt1->next)//调用已经注册的所有文件格式函数来识别
    {
        if (!is_opened)
            continue;

        score = 0;
        if (fmt1->read_probe)//调用某个文件格式函数的read_probe函数检测识别
        {
            score = fmt1->read_probe(pd);//如果能识别到的话score=100，否则可能为0
        }
        else if (fmt1->extensions)//再调用某个文件格式函数的扩展名，看是否匹配
        {
            if (match_ext(pd->filename, fmt1->extensions))//匹配的话score=50
                score = 50;
        }
        if (score > score_max)//找到最大匹配的格式，若所有都不匹配score=0, if不成立
        {
            score_max = score;
            fmt = fmt1;
        }
    }
    return fmt;
}
//识别媒体流格式，设置AVFormatContext
int av_open_input_stream(AVFormatContext **ic_ptr, ByteIOContext *pb, const char *filename,
						 AVInputFormat *fmt, AVFormatParameters *ap)
{
    int err;
    AVFormatContext *ic;
    AVFormatParameters default_ap;

    if (!ap)
    {
        ap = &default_ap;
        memset(ap, 0, sizeof(default_ap));
    }

    ic = av_mallocz(sizeof(AVFormatContext));
    if (!ic)
    {
        err = AVERROR_NOMEM;
        goto fail;
    }
    ic->iformat = fmt;//设置AVFormatContext
    if (pb)
        ic->pb =  *pb;//设置AVFormatContext

    if (fmt->priv_data_size > 0)
    {
        ic->priv_data = av_mallocz(fmt->priv_data_size);//设置AVFormatContext
        if (!ic->priv_data)
        {
            err = AVERROR_NOMEM;
            goto fail;
        }
    }
    else
    {
        ic->priv_data = NULL;//设置AVFormatContext
    }

    err = ic->iformat->read_header(ic, ap);//读取文件头，识别媒体流格式，设置AVFormatContext
    if (err < 0)
        goto fail;

    *ic_ptr = ic;//设置AVFormatContext完成
    return 0;

fail: 
	if (ic)
        av_freep(&ic->priv_data);

    av_free(ic);
    *ic_ptr = NULL;
    return err;
}

int av_open_input_file(AVFormatContext **ic_ptr, const char *filename, AVInputFormat *fmt,
					   int buf_size, AVFormatParameters *ap)
{
    int err, must_open_file, file_opened, probe_size;
    AVProbeData probe_data,  *pd = &probe_data;
    ByteIOContext pb1,  *pb = &pb1;

    file_opened = 0;
    pd->filename = "";
    if (filename)
        pd->filename = filename;//探测数据AVProbeData成员filename
    pd->buf = NULL;
    pd->buf_size = 0;

    must_open_file = 1;

    if (!fmt || must_open_file)//如果未指定输入文件格式fmt，则探测其格式
    {
        if (url_fopen(pb, filename, URL_RDONLY) < 0)//广义url_fopen，这里是本地文件file，初始化pb
        {
            err = AVERROR_IO;
            goto fail;
        }
        file_opened = 1;
        if (buf_size > 0)//如果指定buf大小，则分配其广义缓冲区pb的大小为buf_size的空间
            url_setbufsize(pb, buf_size);

        for (probe_size = PROBE_BUF_MIN; probe_size <= PROBE_BUF_MAX && !fmt; probe_size <<= 1)
        {
            pd->buf = av_realloc(pd->buf, probe_size);
            pd->buf_size = url_fread(pb, pd->buf, probe_size);//从广义缓冲区pb读取大小为probe_size数据到探测数据区pd
            if (url_fseek(pb, 0, SEEK_SET) == (offset_t) - EPIPE)
            {
                url_fclose(pb);
                if (url_fopen(pb, filename, URL_RDONLY) < 0)
                {
                    file_opened = 0;
                    err = AVERROR_IO;
                    goto fail;
                }
            }

            fmt = av_probe_input_format(pd, 1);//探测格式函数
        }
        av_freep(&pd->buf);
    }

    if (!fmt)
    {
        err = AVERROR_NOFMT;
        goto fail;
    }
	//知道其文件格式之后,识别媒体流格式
    err = av_open_input_stream(ic_ptr, pb, filename, fmt, ap);
    if (err)
        goto fail;
    return 0;

fail:
	av_freep(&pd->buf);
    if (file_opened)
        url_fclose(pb);
    *ic_ptr = NULL;
    return err;
}
//读出一个包的数据，这里是一帧数据
int av_read_packet(AVFormatContext *s, AVPacket *pkt)
{
    return s->iformat->read_packet(s, pkt);
}

int av_add_index_entry(AVStream *st, int64_t pos, int64_t timestamp, int size, int distance, int flags)
{
    AVIndexEntry *entries,  *ie;
    int index;

    if ((unsigned)st->nb_index_entries + 1 >= UINT_MAX / sizeof(AVIndexEntry)) // 越界判断
        return  - 1;

    entries = av_fast_realloc(st->index_entries, &st->index_entries_allocated_size, 
		            (st->nb_index_entries + 1) * sizeof(AVIndexEntry));
    if (!entries)
        return  - 1;

    st->index_entries = entries;

    index = av_index_search_timestamp(st, timestamp, AVSEEK_FLAG_ANY);

    if (index < 0)	// 后续
    {
        index = st->nb_index_entries++;
        ie = &entries[index];
        assert(index == 0 || ie[ - 1].timestamp < timestamp);
    }
    else			// 中插
    {
        ie = &entries[index];
        if (ie->timestamp != timestamp)
        {
            if (ie->timestamp <= timestamp)
                return  - 1;

            memmove(entries + index + 1, entries + index, 
				             sizeof(AVIndexEntry)*(st->nb_index_entries - index));

            st->nb_index_entries++;
        }
    }

    ie->pos = pos;
    ie->timestamp = timestamp;
    ie->size = size;
    ie->flags = flags;

    return index;
}

int av_index_search_timestamp(AVStream *st, int64_t wanted_timestamp, int flags) 
{
    AVIndexEntry *entries = st->index_entries;
    int nb_entries = st->nb_index_entries;
    int a, b, m;
    int64_t timestamp;

    a =  - 1;
    b = nb_entries;

    while (b - a > 1) //并没有记录idx值，采用的是折半查找
    {
        m = (a + b) >> 1;
        timestamp = entries[m].timestamp;
        if (timestamp >= wanted_timestamp)
            b = m;
        if (timestamp <= wanted_timestamp)
            a = m;
    }

    m = (flags &AVSEEK_FLAG_BACKWARD) ? a : b;

    if (!(flags &AVSEEK_FLAG_ANY))
    {
        while (m >= 0 && m < nb_entries && !(entries[m].flags &AVINDEX_KEYFRAME))
        {
            m += (flags &AVSEEK_FLAG_BACKWARD) ?  - 1: 1;
        }
    }

    if (m == nb_entries)
        return  - 1;

    return m;
}

void av_close_input_file(AVFormatContext *s)
{
    int i;
    AVStream *st;

    if (s->iformat->read_close)
        s->iformat->read_close(s);

    for (i = 0; i < s->nb_streams; i++)
    {
        st = s->streams[i];
        av_free(st->index_entries);
        av_free(st->actx);
        av_free(st);
    }

    url_fclose(&s->pb);

    av_freep(&s->priv_data);
    av_free(s);
}

AVStream *av_new_stream(AVFormatContext *s, int id)
{
    AVStream *st;

    if (s->nb_streams >= MAX_STREAMS)
        return NULL;

    st = av_mallocz(sizeof(AVStream));
    if (!st)
        return NULL;

    st->actx = avcodec_alloc_context();

    s->streams[s->nb_streams++] = st;
    return st;
}

void av_set_pts_info(AVStream *s, int pts_wrap_bits, int pts_num, int pts_den)
{
    s->time_base.num = pts_num;
    s->time_base.den = pts_den;
}
