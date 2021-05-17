#ifndef poll_socket_epoll_h
#define poll_socket_epoll_h

#include <netdb.h>
#include <unistd.h>
#include <sys/epoll.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <fcntl.h>

static bool 
sp_invalid(int efd) {
	return efd == -1;
}

static int
sp_create() {
	return epoll_create(1024);
	/*
	 * epoll_create(int size);
	 * 创建一个epoll的句柄，size用来告诉内核这个监听的数目一共有多大。这个参数不同于select()中的第一个参数，
	 * 给出最大监听的fd+1的值。需要注意的是，当创建好epoll句柄后，它就是会占用一个fd值，在linux下如果查看/proc/进程id/fd/，
	 * 是能够看到这个fd的，所以在使用完epoll后，必须调用close()关闭，否则可能导致fd被耗尽。
	 */
}

static void
sp_release(int efd) {
	close(efd);
}
/*
 * epoll_ctl是epoll的事件注册函数，它不同与select()是在监听事件时告诉内核要监听什么类型的事件，而是在这里先注册要监听的事件类型。
 * 第一个参数是epoll_create()的返回值，第二个参数表示动作，用三个宏来表示
 * EPOLLIN ：表示对应的文件描述符可以读（包括对端SOCKET正常关闭）；
 * EPOLLOUT：表示对应的文件描述符可以写；
 * EPOLLPRI：表示对应的文件描述符有紧急的数据可读（这里应该表示有带外数据到来）；
 * EPOLLERR：表示对应的文件描述符发生错误；
 * EPOLLHUP：表示对应的文件描述符被挂断；
 * EPOLLET： 将EPOLL设为边缘触发(Edge Triggered)模式，这是相对于水平触发(Level Triggered)来说的。
 * EPOLLONESHOT：只监听一次事件，当监听完这次事件之后，如果还需要继续监听这个socket的话，需要再次把这个socket加入到EPOLL队列里
 */

static int 
sp_add(int efd, int sock, void *ud) {
	struct epoll_event ev;
	ev.events = EPOLLIN;
	ev.data.ptr = ud;
    // EPOLL_CTL_ADD：注册新的fd到epfd中；
	if (epoll_ctl(efd, EPOLL_CTL_ADD, sock, &ev) == -1) {
		return 1;
	}
	return 0;
}

static void 
sp_del(int efd, int sock) {
    // EPOLL_CTL_DEL：从epfd中删除一个fd；
	epoll_ctl(efd, EPOLL_CTL_DEL, sock , NULL);
}

static void 
sp_write(int efd, int sock, void *ud, bool enable) {
	struct epoll_event ev;
	ev.events = EPOLLIN | (enable ? EPOLLOUT : 0);
	ev.data.ptr = ud;
	// EPOLL_CTL_MOD：修改已经注册的fd的监听事件；
	epoll_ctl(efd, EPOLL_CTL_MOD, sock, &ev);
}

static int 
sp_wait(int efd, struct event *e, int max) {
	struct epoll_event ev[max];
	// epoll_wait：等待事件的产生，类似于select()调用。参数events用来从内核得到事件的集合，maxevents告之内核这个events有多大，
	// 这个 maxevents的值不能大于创建epoll_create()时的size，参数timeout是超时时间（毫秒，0会立即返回，-1将不确定，也有说法说是永久阻塞）。
	// 该函数返回需要处理的事件数目，如返回0表示已超时。
	int n = epoll_wait(efd , ev, max, -1);
	int i;
	for (i=0;i<n;i++) {
		e[i].s = ev[i].data.ptr;
		unsigned flag = ev[i].events;
		e[i].write = (flag & EPOLLOUT) != 0;
		e[i].read = (flag & (EPOLLIN | EPOLLHUP)) != 0;
		e[i].error = (flag & EPOLLERR) != 0;
		e[i].eof = false;
	}

	return n;
}

static void
sp_nonblocking(int fd) {
	int flag = fcntl(fd, F_GETFL, 0);
	if ( -1 == flag ) {
		return;
	}

	fcntl(fd, F_SETFL, flag | O_NONBLOCK);
}

#endif
