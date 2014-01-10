#include <QCoreApplication>

#include <kovanserial/usb_serial.hpp>
#include <kovanserial/tcp_server.hpp>
#include <kovanserial/command_types.hpp>
#include <kovanserial/platform_defines.hpp>
#include <kovanserial/kovan_serial.hpp>

#include "server_thread.hpp"
#include "tcp_server_thread.hpp"
#include "heartbeat.hpp"
#include "serial_bridge.hpp"

#include <cstdlib>
#include <cstdio>
#include <unistd.h>

// #define DEV_MODE


int main(int argc, char *argv[])
{
	QCoreApplication app(argc, argv);
	
	char serialPort[128];
	if(argc == 2) strncpy(serialPort, argv[1], 128);
	else strncpy(serialPort, "/dev/ttyGS0", 128);
	
	ServerThread *providers[2] = {0, 0};
	
#ifndef DEV_MODE
	UsbSerial usb(serialPort);
	// This may execute before the device is ready for opening.
	// Wait until it is ready.
	while(!usb.makeAvailable()) {
		perror("open");
		sleep(2);
	}
	providers[0] = new ServerThread(&usb);
#endif
	
	TcpServer server;
	server.bind(KOVAN_SERIAL_PORT);
	server.listen(2);
	if(server.makeAvailable()) providers[1] = new TcpServerThread(&server);
	else perror("tcp");
	
  SerialBridge bridge;
	
	for(int i = 0; i < 2; ++i) {
		if(!providers[i]) continue;
		QObject::connect(providers[i], SIGNAL(run(QString)), &bridge, SLOT(run(QString)));
		providers[i]->start();
	}
	
	Heartbeat *heart = new Heartbeat();
	int ret = app.exec();
	delete heart;
	
	for(int i = 0; i < 2; ++i) {
		if(!providers[i]) continue;
		providers[i]->stop();
		providers[i]->wait();
		delete providers[i];
	}
	
#ifndef DEV_MODE
	usb.endSession();
#endif
	server.endSession();
	
	return ret;
}
