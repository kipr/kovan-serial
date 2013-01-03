#include "tcp_server_thread.hpp"

#include <kovanserial/tcp_server.hpp>
#include <kovanserial/kovan_serial.hpp>
#include <QDebug>

TcpServerThread::TcpServerThread(TcpServer *transmitter)
	: ServerThread(transmitter)
{
}

void TcpServerThread::run()
{
	Packet p;
	while(!isStopping()) {
		QThread::msleep(100);
		if(!dynamic_cast<TcpServer *>(transmitter())->accept(1)) continue;
		while(proto()->next(p, 5000) && handle(p));
	}
}