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
		QThread::yieldCurrentThread();
		if(!dynamic_cast<TcpServer *>(transmitter())->accept(3000)) continue;
		while(proto()->next(p, 5000) && handle(p));
	}
}