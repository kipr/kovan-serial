#include <QCoreApplication>

#include <kovanserial/usb_serial.hpp>
#include <kovanserial/tcp_server.hpp>
#include <kovanserial/command_types.hpp>
#include <kovanserial/platform_defines.hpp>
#include <kovanserial/kovan_serial.hpp>

#include "server_thread.hpp"
#include "tcp_server_thread.hpp"
#include "heartbeat.hpp"
#include "Serial.h"

#include <cstdlib>
#include <cstdio>
#include <unistd.h>

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
	// This may execute before the device is ready for opening.
	// Wait until it is ready.
	while(!usb.makeAvailable()) {
		perror("open");
		sleep(2);
	}
	providers[0] = new ServerThread(&usb);
	
	TcpServer server;
	server.bind(KOVAN_SERIAL_PORT);
	server.listen(2);
	if(server.makeAvailable()) providers[1] = new TcpServerThread(&server);
	else perror("tcp");
	
	QDBusConnection::sessionBus().registerService("org.kipr.Serial");
	Serial *serialDBus = new Serial("org.kipr.Serial", "/org/kipr/Serial", QDBusConnection::systemBus());
	
	for(int i = 0; i < 2; ++i) {
		if(!providers[i]) continue;
		QObject::connect(providers[i], SIGNAL(run(QString)), serialDBus, SIGNAL(Run(QString)));
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
	
	delete serialDBus;
	
	usb.endSession();
	server.endSession();
	
	return ret;
}