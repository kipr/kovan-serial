#include <QCoreApplication>

#include <kovanserial/usb_serial.hpp>
#include <kovanserial/tcp_server.hpp>
#include <kovanserial/command_types.hpp>
#include <kovanserial/platform_defines.hpp>
#include <kovanserial/kovan_serial.hpp>

#include "server_thread.hpp"
#include "tcp_server_thread.hpp"
#include "heartbeat.hpp"

#include <cstdlib>
#include <cstdio>

std::string property(const std::string &prop)
{
	if(!prop.compare(KOVAN_PROPERTY_DISPLAY_NAME)) return "betabot";
	if(!prop.compare(KOVAN_PROPERTY_SERIAL)) return "1234";
	if(!prop.compare(KOVAN_PROPERTY_VERSION)) return "0.1";
	if(!prop.compare(KOVAN_PROPERTY_DEVICE)) return "kovan";
	return "";
}

int main(int argc, char *argv[])
{
	QCoreApplication app(argc, argv);
	
	char serialPort[128];
	if(argc == 2) strncpy(serialPort, argv[1], 128);
	else strncpy(serialPort, "/dev/ttyGS0", 128);
	
	ServerThread *providers[2] = {0, 0};
	
	UsbSerial usb(serialPort);
	if(usb.makeAvailable()) providers[0] = new ServerThread(&usb);
	else perror("open");
	
	TcpServer server;
	server.bind(KOVAN_SERIAL_PORT);
	server.listen(2);
	if(server.makeAvailable()) providers[1] = new TcpServerThread(&server);
	else perror("tcp");
	
	for(int i = 0; i < 2; ++i) {
		if(!providers[i]) continue;
		providers[i]->start();
	}
	
	Heartbeat heart;
	
	
	int ret = app.exec();
	
	for(int i = 0; i < 2; ++i) {
		if(!providers[i]) continue;
		providers[i]->stop();
		providers[i]->wait();
		delete providers[i];
	}
	
	usb.endSession();
	server.endSession();
	
	return ret;
}