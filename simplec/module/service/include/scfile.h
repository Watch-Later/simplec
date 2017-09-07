#ifndef _H_SIMPLEC_SCFILE
#define _H_SIMPLEC_SCFILE

#include <schead.h>

//
// file_mtime - �õ��ļ�����޸�ʱ��
// path     : �ļ�����
// return   : -1 ��ʾ��ȡ����, �������ʱ���
//
extern time_t file_mtime(const char * path);

//
// file_set - ������Ҫ���µ��ļ�����
// path     : �ļ�·��
// func     : NULL ��ʾ���, ���� func(arg, path -> FILE)
// arg      : ע��Ķ������
// return   : void
//
extern void file_set_(const char * path, void * arg, 
                      void (* func)(void * arg, FILE * cnf));
#define file_set(path, arg, func) \
        file_set_(path, arg, (void (*)(void *, FILE *))func)

//
// file_update - ����ע�����ý����¼�
// return   : void
//
extern void file_update(void);

//
// file_delete - ����Ѿ�ע���, ��Ҫ�� update ֮ǰ����֮��
// return   : void
//
void file_delete(void);

#endif//_H_SIMPLEC_SCFILE
