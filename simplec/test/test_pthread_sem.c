#include <stdio.h>
#include <stdlib.h>
#include <pthread.h>
#include <semaphore.h>

// �����߳�����
#define _INT_THS	(3)

struct threadarg {
	pthread_t tids[_INT_THS];
	sem_t sids[_INT_THS];
};

// �����к���
static void * _run(void * arg) {
	int i = -1, j = -1;
	struct threadarg * ths = arg;
	pthread_t tid = pthread_self();
	pthread_detach(tid);

	// ȷ�����ǵڼ����߳�
	while (++i < _INT_THS)
		if (pthread_equal(tid, ths->tids[i]))
			break;
	
	// ѭ�����ض������ͽ�����
	while (++j < _INT_THS) {
		// �� i ���߳�, �ȴ� �� i - 1 ���߳�, ��� 'A' + i 
		sem_wait(ths->sids + (i - 1 + _INT_THS) % _INT_THS);
		putchar('A' + i);
		// �� i ���߳�, ���� �� i ���ź���
		sem_post(ths->sids + i);
	}

	return NULL;
}

//
// д�������߳��ź�������
// ���� _INT_THS ���߳�, ˳���ӡ���� A->B->C->...->A->B->....
//
void test_pthread_sem(void) {
	// ��ʼ��ʼ����
	int i, j;
	struct threadarg targ;
	
	// �ȳ�ʼ���ź���,���ʼ���߳�
	for (i = 0; i < _INT_THS; ++i) {
		// 0 : ��ʾ�ֲ��ź�����ǰ����; 0 : ��ǰ�ź���ֵΪ0
		if (sem_init(targ.sids + i, 0, 0) < 0)
			goto __for_exit;
	}

	// �����߳�
	for (j = 0; j < _INT_THS; ++j) {
		// ���������߳�
		if (pthread_create(targ.tids + j, NULL, _run, &targ) < 0)
			goto __for_exit;
	}

	// �����һ���߳�, ��� 'A' ��ͷ
	sem_post(targ.sids + _INT_THS - 1);

	// �м�ȴ�һЩʱ���
	getchar();

__for_exit:
	// ע�����, �����ź����ͷ���, �̻߳�����, ���쳣
	for (j = 0; j < i; ++j)
		sem_destroy(targ.sids + j);
#ifdef __GNUC__
	exit(EXIT_SUCCESS);
#endif
}