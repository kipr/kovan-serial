#include "tcp_server_thread.hpp"

#include <kovanserial/tcp_server.hpp>
#include <kovanserial/transport_layer.hpp>
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
		for(;;) {
			TransportLayer::Return ret = proto()->next(p, 5000);
			if(ret == TransportLayer::Success && handle(p)); //std::cout << "Handled trusted command" << std::endl;
			else if(ret == TransportLayer::UntrustedSuccess && handleUntrusted(p)); //std::cout << "Handled untrusted command" << std::endl;
			else break;
		}
	}
}