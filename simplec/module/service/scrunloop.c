﻿#include <mq.h>
#include <schead.h>
#include <scrunloop.h>

// 具体轮询器
struct srl {
	pthread_t id;			// 具体奔跑的线程
	pthread_mutex_t mtx;	// 条件变量的锁
	pthread_cond_t cond;	// 条件变量

	mq_t mq;				// 消息队列
	node_f run;				// 每个消息都会调用 run(pop())
	node_f die;				// 每个消息体的善后工作
	volatile bool loop;		// true表示还在继续 
	volatile bool wait;		// true表示当前轮序器正在等待
};

static void * _srl_loop(struct srl * s) {
	pthread_mutex_lock(&s->mtx);
	while (s->loop) {
		void * pop = mq_pop(s->mq);
		if (NULL == pop) {
			s->wait = true;
			pthread_cond_wait(&s->cond, &s->mtx);
		} else {
			// 开始处理消息
			s->run(pop);
			s->die(pop);
		}
	}
	pthread_mutex_unlock(&s->mtx);
	return s;
}

//
// srl_create - 创建轮询服务对象
// run		: 轮序处理每条消息体, 弹出消息体的时候执行
// die		: srl_push msg 的销毁函数
// return	: void 
//
srl_t
srl_create_(node_f run, node_f die) {
	struct srl * s = malloc(sizeof(struct srl));
	assert(s && run);
	s->mq = mq_create();
	s->loop = true;
	s->run = run;
	s->die = die;
	s->wait = true;
	pthread_mutex_init(&s->mtx, NULL);
	pthread_cond_init(&s->cond, NULL);
	// 创建线程,并启动
	if (pthread_create(&s->id, NULL, (start_f)_srl_loop, s)) {
		mq_delete(s->mq, die);
		free(s);
		RETURN(NULL, "pthread_create create error !!!");
	}
	return s;
}

//
// srl_delete - 销毁轮询对象,回收资源
// s		: 轮询对象
// return	: void 
//
void
srl_delete(srl_t s) {
	if (s) {
		s->loop = false;
		pthread_cond_signal(&s->cond);
		// 等待线程结束, 然后退出
		pthread_join(s->id, NULL);
		pthread_mutex_destroy(&s->mtx);
		pthread_cond_destroy(&s->cond);
		mq_delete(s->mq, s->die);
		free(s);
	}
}

//
// srl_push - 将消息压入到轮询器中
// s		: 轮询对象
// msg		: 待加入的消息地址
// return	: void
// 
inline void 
srl_push(srl_t s, void * msg) {
	assert(s && msg);
	mq_push(s->mq, msg);
	if (s->wait) {
		s->wait = false;
		pthread_cond_signal(&s->cond);
	}
}