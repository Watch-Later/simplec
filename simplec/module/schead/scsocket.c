﻿#include <scsocket.h>

#ifdef _MSC_VER
static inline void _socket_start(void) {
	WSACleanup();
}
#endif

//
// socket_start	- 单例启动socket库的初始化方法
// socket_addr	- 通过ip, port 得到 ipv4 地址信息
// 
inline void 
socket_start(void) {
#ifdef _MSC_VER
	WSADATA wsad;
	WSAStartup(WINSOCK_VERSION, &wsad);
	atexit(_socket_start);
#else
	IGNORE_SIGPIPE();
#endif
}

int 
socket_addr(const char * ip, uint16_t port, sockaddr_t * addr) {
	if (!ip || !*ip || !addr)
		RETURN(Error_Param, "check empty ip = %s, port = %hu, addr = %p.", ip, port, addr);

	addr->sin_family = AF_INET;
	addr->sin_port = htons(port);
	addr->sin_addr.s_addr = inet_addr(ip);
	if (addr->sin_addr.s_addr == INADDR_NONE) {
		struct hostent * host = gethostbyname(ip);
		if (!host || !host->h_addr)
			RETURN(Error_Param, "check ip is error = %s.", ip);
		// 尝试一种, 默认ipv4
		memcpy(&addr->sin_addr, host->h_addr, host->h_length);
	}
	memset(addr->sin_zero, 0, sizeof addr->sin_zero);

	return Success_Base;
}

//
// socket_dgram		- 创建UDP socket
// socket_stream	- 创建TCP socket
// socket_close		- 关闭上面创建后的句柄
// socket_bind		- socket绑定操作
//

inline socket_t
socket_dgram(void) {
	return socket(PF_INET, SOCK_DGRAM, IPPROTO_UDP);
}

inline socket_t
socket_stream(void) {
	return socket(PF_INET, SOCK_STREAM, IPPROTO_TCP);
}

int
socket_close(socket_t s) {
	int r;
#ifdef _MSC_VER
	r = closesocket(s);
#else
	do {
		r = close(s);
	} while (r == SOCKET_ERROR && socket_errno == SOCKET_EINTR);
#endif
	return r;
}

inline int
socket_bind(socket_t s, const char * ip, uint16_t port) {
	sockaddr_t addr = { AF_INET };
	addr.sin_port = htons(port);
	if (!ip || !*ip || !strcmp(ip, "0.0.0.0"))
		addr.sin_addr.s_addr = INADDR_ANY;
	else
		addr.sin_addr.s_addr = inet_addr(ip);
	return bind(s, (const struct sockaddr *)&addr, sizeof addr);
}

//
// socket_set_block		- 设置套接字是阻塞
// socket_set_nonblock	- 设置套接字是非阻塞
// socket_set_reuseaddr	- 开启地址复用
// socket_set_recvtimeo	- 设置接收数据毫秒超时时间
// socket_set_sendtimeo	- 设置发送数据毫秒超时时间
//

inline int
socket_set_block(socket_t s) {
#ifdef _MSC_VER
	u_long mode = 0;
	return ioctlsocket(s, FIONBIO, &mode);
#else
	int mode = fcntl(s, F_GETFL, 0);
	if (mode == SOCKET_ERROR)
		return SOCKET_ERROR;
	if (mode & O_NONBLOCK)
		return fcntl(s, F_SETFL, mode & ~O_NONBLOCK);
	return Success_Base;
#endif	
}

inline int
socket_set_nonblock(socket_t s) {
#ifdef _MSC_VER
	u_long mode = 1;
	return ioctlsocket(s, FIONBIO, &mode);
#else
	int mode = fcntl(s, F_GETFL, 0);
	if (mode == SOCKET_ERROR)
		return SOCKET_ERROR;
	if (mode & O_NONBLOCK)
		return Success_Base;
	return fcntl(s, F_SETFL, mode | O_NONBLOCK);
#endif	
}

inline int
socket_set_reuseaddr(socket_t s) {
	int ov = 1;
	return setsockopt(s, SOL_SOCKET, SO_REUSEADDR, (const char *)&ov, sizeof ov);
}

inline int
socket_set_recvtimeo(socket_t s, int ms) {
	struct timeval tv;
	MAKE_TIMEVAL(tv, ms);
	return setsockopt(s, SOL_SOCKET, SO_RCVTIMEO, (const char *)&tv, sizeof tv);
}

int
socket_set_sendtimeo(socket_t s, int ms) {
	struct timeval tv;
	MAKE_TIMEVAL(tv, ms);
	return setsockopt(s, SOL_SOCKET, SO_SNDTIMEO, (const char *)&tv, sizeof tv);;
}

//
// socket_recv		- socket接受信息
// socket_recvn		- socket接受len个字节进来
// socket_send		- socket发送消息
// socket_sendn		- socket发送len个字节出去
// socket_recvfrom	- recvfrom接受函数
// socket_sendto	- udp发送函数, 通过socket_udp 搞的完全可以 socket_send发送
//

int
socket_recv(socket_t s, void * buf, int len) {
	int r;
	do {
		r = recv(s, buf, len, 0);
	} while (r == SOCKET_ERROR && socket_errno == SOCKET_EINTR);
	return r;
}

int
socket_recvn(socket_t s, void * buf, int len) {
	int r, nlen = len;
	while (nlen > 0) {
		r = socket_recv(s, buf, nlen);
		if (r == 0) break;
		if (r == SOCKET_ERROR) {
			RETURN(SOCKET_ERROR, "socket_recv SOCKET_ERROR s = %"PRIu64", len = %d, nlen = %d.", (uint64_t)s, len, nlen);
		}
		nlen -= r;
		buf = (char *)buf + r;
	}
	return len - nlen;
}

int
socket_send(socket_t s, const void * buf, int len) {
	int r;
	do {
		r = send(s, buf, len, 0);
	} while (r == SOCKET_ERROR && socket_errno == SOCKET_EINTR);
	return r;
}

int
socket_sendn(socket_t s, const void * buf, int len) {
	int r, nlen = len;
	while (nlen > 0) {
		r = socket_send(s, buf, nlen);
		if (r == 0) break;
		if (r == SOCKET_ERROR) {
			RETURN(SOCKET_ERROR, "socket_send SOCKET_ERROR s = %"PRIu64", len = %d, nlen = %d.", (uint64_t)s, len, nlen);
		}
		nlen -= r;
		buf = (const char *)buf + r;
	}
	return len - nlen;
}

inline int
socket_recvfrom(socket_t s, void * buf, int len, int flags, sockaddr_t * in, socklen_t * inlen) {
	return recvfrom(s, buf, len, flags, (struct sockaddr *)in, inlen);
}

inline int
socket_sendto(socket_t s, const void * buf, int len, int flags, const sockaddr_t * to, socklen_t tolen) {
	return sendto(s, buf, len, flags, (const struct sockaddr *)to, tolen);
}

//
// socket_tcp			- 创建TCP详细的套接字
// socket_udp			- 创建UDP详细套接字
// socket_connect		- connect操作
// socket_connects		- 通过socket地址连接
// socket_connecto		- connect连接超时判断
// socket_connectos		- connect连接客户端然后返回socket_t句柄
// socket_accept		- accept 链接函数
//

socket_t
socket_tcp(const char * host, uint16_t port) {
	int r;
	socket_t s = socket_stream();
	if (INVALID_SOCKET == s) {
		RETURN(INVALID_SOCKET, "socket_stream socket error s = %"PRIu64".", (uint64_t)s);
	}

	// 开启地址复用和端口绑定
	r = socket_set_reuseaddr(s);
	r |= socket_bind(s, host, port);
	if (r != Success_Base) {
		socket_close(s);
		RETURN(INVALID_SOCKET, "socket reuseaddr or bind is error s = %"PRIu64".", (uint64_t)s);
	}

	// 开启监听
	r = listen(s, SOMAXCONN);
	if (r != Success_Base) {
		socket_close(s);
		RETURN(INVALID_SOCKET, "socket listen SOMAXCONN is error s = %"PRIu64".", (uint64_t)s);
	}

	return s;
}

socket_t
socket_udp(const char * host, uint16_t port) {
	int r;
	socket_t s = socket_dgram();
	if (INVALID_SOCKET == s) {
		RETURN(INVALID_SOCKET, "socket_dgram socket error s = %"PRIu64".", (uint64_t)s);
	}

	// 开启地址复用和端口绑定
	r = socket_set_reuseaddr(s);
	r |= socket_bind(s, host, port);
	if (r != Success_Base) {
		socket_close(s);
		RETURN(INVALID_SOCKET, "socket reuseaddr or bind is error s = %"PRIu64".", (uint64_t)s);
	}
	return s;
}

inline int
socket_connect(socket_t s, const sockaddr_t * addr) {
	return connect(s, (const struct sockaddr *)addr, sizeof(*addr));
}

inline int
socket_connects(socket_t s, const char * ip, uint16_t port) {
	sockaddr_t addr;
	int r = socket_addr(ip, port, &addr);
	if (r < Success_Base)
		return r;
	return socket_connect(s, &addr);
}

int
socket_connecto(socket_t s, const sockaddr_t * addr, int ms) {
	int n, r;
	struct timeval to;
	fd_set rset, wset;

	// 还是阻塞的connect
	if (ms < 0) return socket_connect(s, addr);

	// 非阻塞登录, 先设置非阻塞模式
	r = socket_set_nonblock(s);
	if (r < Success_Base) {
		RETURN(r, "socket_set_nonblock error s = %"PRIu64".", (uint64_t)s);
	}

	// 尝试连接一下, 非阻塞connect 返回 -1 并且 errno == EINPROGRESS 表示正在建立链接
	r = socket_connect(s, addr);
	if (r >= Success_Base) goto __return;

	// 链接还在进行中, linux这里显示EINPROGRESS，windows应该是EWOULDBLOCK
#ifdef _MSC_VER
	if (socket_errno != SOCKET_EWOULDBOCK) {
#else
	if (socket_errno != SOCKET_EINPROGRESS) {
#endif
		CERR("socket_connect error errno = %d, s = %"PRIu64".", socket_errno, (uint64_t)s);
		goto __return;
	}

	// 超时 timeout, 直接返回结果 Error_Base = -1 错误
	r = Error_Base;
	if (ms == 0) goto __return;

	FD_ZERO(&rset);
	FD_SET(s, &rset);
	FD_ZERO(&wset);
	FD_SET(s, &wset);
	MAKE_TIMEVAL(to, ms);
	n = select((int)s + 1, &rset, &wset, NULL, &to);
	// 超时直接滚
	if (n <= 0) goto __return;

	// 当连接成功时候,描述符会变成可写
	if (n == 1 && FD_ISSET(s, &wset)) {
		r = Success_Base;
		goto __return;
	}

	// 当连接建立遇到错误时候, 描述符变为即可读又可写
	if (n == 2) {
		socklen_t len = sizeof n;
		// 只要最后没有 error那就 链接成功
		if (!getsockopt(s, SOL_SOCKET, SO_ERROR, (char *)&n, &len) && !n)
			r = Success_Base;
	}

__return:
	socket_set_block(s);
	return r;
}

socket_t
socket_connectos(const char * host, uint16_t port, int ms) {
	int r;
	sockaddr_t addr;
	socket_t s = socket_stream();
	if (s == INVALID_SOCKET) {
		RETURN(INVALID_SOCKET, "socket_stream is error!");
	}

	// 构建ip地址
	r = socket_addr(host, port, &addr);
	if (r < Success_Base)
		return r;

	r = socket_connecto(s, &addr, ms);
	if (r < Success_Base) {
		socket_close(s);
		RETURN(INVALID_SOCKET, "socket_connecto host port ms = %s, %u, %d!", host, port, ms);
	}

	return s;
}

inline socket_t
socket_accept(socket_t s, sockaddr_t * addr, socklen_t * len) {
	return accept(s, (struct sockaddr *)addr, len);
}