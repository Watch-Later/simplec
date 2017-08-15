#include <hashid.h>
#include <pthread.h>
#include <scrunloop.h>
#include <socket_start.h>

#define _INT_CNMAX (4096 - 1)

struct connect {
	int id;	// server socket id
	rsmq_t buffer;
};

struct gate {
	intptr_t opaque;
	int lisid;
	struct hashid hash;
	struct connect conn[_INT_CNMAX];
	srl_t mloop;
};

static void _gate_delete(struct gate * g) {
	intptr_t ctx = g->opaque;
	for (int i = 0; i < LEN(g->conn); ++i) {
		struct connect * c = g->conn + i;
		if (c->id >= 0) {
			rsmq_delete(c->buffer);
			server_close(ctx, c->id);
		}
	}
	if (g->lisid >= 0) 
		server_close(ctx, g->lisid);
	hashid_clear(&g->hash);
	free(g);
}

static struct gate * _gate_create(const char * host, uint16_t port) {
	struct gate * g = malloc(sizeof(struct gate));
	if (NULL == g)
		RETURN(NULL, "malloc sizeof struct gate error!");
	g->opaque = (intptr_t)g;
	for (int i = 0; i < LEN(g->conn); ++i) {
		g->conn[i].id = -1;
		g->conn[i].buffer = NULL;
	}

	hashid_init(&g->hash, LEN(g->conn));
	// ���濪ʼ������������
	g->lisid = server_listen(g->opaque, host, port);
	if (g->lisid < 0) {
		free(g);
		RETURN(NULL, "server_listen host = %s, port = %hu err.", host, port);
	}
	server_start(g->opaque, g->lisid);
	return g;
}

// socket poll ���������
static void * _server(void * arg) {

	for (;;) {
		int r = server_poll();
		if (r == 0)
			break;
		if (r < 0) {
			// check context is empty need break exit
			continue;
		}
		// awaken block thread 
	}
	
	return NULL;
}

//
// ss_run - ����socket ������
// host		: ��������
// port		: �˿�
// run		: ��Ϣ����Э��
// return	: void
//
extern void ss_run(const char * host, uint16_t port, void (* run)(msgrs_t)) {
	pthread_t tid;
	struct gate * g;

	server_init();
	g = _gate_create(host, port);
	if (g == NULL)
		CERR_EXIT("gate_create err host = %s, port = %hu.", host, port);

	g->mloop = srl_create(run, msgrs_delete);

	// ���￪ʼ�����߳�������
	if (pthread_create(&tid, NULL, _server, NULL))
		CERR_EXIT("pthread_create is error!");

	// �ȴ��߳̽���
	pthread_join(tid, NULL);
	_gate_delete(g);
	server_free();
}

static void _ss_push_data(uintptr_t opaque, struct smsg * sm) {
	int id, r;
	msgrs_t msg;
	struct gate * g = (struct gate *)opaque;
	id = hashid_lookup(&g->hash, sm->id);
	if (id > 0) {
		struct connect * c = g->conn + id;
		// �������
		rsmq_push(c->buffer, sm->data, sm->ud);
		// ... ��������Ҫ����Ϣ��, ����ȥ
		r = rsmq_pop(c->buffer, &msg);
		if (r == ErrParse) {
			CL_ERROR("rsmq_pop parse error %d", sm->id);
			server_close(opaque, sm->id);
		} else if (r == SufBase) {
			// ����ѹ���п�ʼ����
			srl_push(g->mloop, msg);
		}

	} else {
		CL_ERROR("Drop unknown connection %d message", sm->id);
		// ���¸����޾����˸�, �����µĶ���Ī���ĸж�~
		server_close(opaque, sm->id);
	}
}

// 
// ss_push - ��socket������������Ϣ
// opaque	: ע��Ķ���
// sm		: �������Ϣ
// return	: void
//
extern void ss_push(uintptr_t opaque, struct smsg * sm) {
	int id;
	struct connect * c;
	struct gate * g = (struct gate *)opaque;
	assert(opaque > 0 && sm);
	switch(sm->type) {
	case SERVER_DATA	:
		_ss_push_data(opaque, sm);
		break;
	case SERVER_CONNECT	:
		// start listening
		if (sm->id == g->lisid)
			break;
		id = hashid_lookup(&g->hash, sm->id);
		if (id < 0) {
			CL_ERROR("Close unknown connection %d", sm->id);
			server_close(opaque, sm->id);
		}
		break;
	case SERVER_CLOSE	:
	case SERVER_ERROR	:
		id = hashid_remove(&g->hash, sm->id);
		if (id >= 0) {
			c = g->conn + id;
			rsmq_delete(c->buffer);
			c->buffer = NULL;
			c->id = -1;
		}
		break;
	case SERVER_ACCEPT	:
		assert(g->lisid == sm->id);
		if(hashid_full(&g->hash)) {
			CL_INFOS("full(%d) hash %d.", sm->id, sm->ud);
			server_close(opaque, sm->id);
		} else {
			id = hashid_insert(&g->hash, sm->ud);
			c = g->conn + id;
			c->id = sm->ud;
			//sm + 1 -> data print
			assert(c->buffer == NULL);
			c->buffer = rsmq_create();
		}
		break;
	case SERVER_UDP		:
		// ĿǰUDP����û������, ��ʱû�п���, ��ʵ������������(��ȫ��UDP�������TCP)
		break;
	case SERVER_WARNING	:
		CL_ERROR("fd (%d) send buffer (%d)K", sm->id, sm->ud);
		break;
	}

	free(sm->data);
	free(sm);
}