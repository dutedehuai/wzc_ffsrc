#include "avformat.h"

extern URLProtocol file_protocol;

void av_register_all(void)
{
    static int inited = 0;

    if (inited != 0)
        return ;
    inited = 1;
    //��ʼ��CPU ��ؼ���ָ��
    avcodec_init();
    //�Ѹ��ֽ�������ӵ�ͷ��Ϊfirst_avcodec��������
    avcodec_register_all();
    //�Ѹ����ļ���ʽ��ӵ�ͷ��Ϊfirst_iformat��������
    avidec_init();
    //�Ѹ����ļ���дЭ����ӵ�ͷ��Ϊfirst_protocol��������
    register_protocol(&file_protocol);
}
