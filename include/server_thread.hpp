#ifndef _SERVER_THREAD_HPP_
#define _SERVER_THREAD_HPP_

#include <QObject>
#include <QString>
#include <QThread>

#include <kar.hpp>
#include <kovanserial/transport_layer.hpp>

class Transmitter;
class KovanSerial;

class ServerThread : public QThread
{
Q_OBJECT
public:
	ServerThread(Transmitter *transmitter);
	~ServerThread();
	
	void stop();
	bool isStopping() const;
	
	virtual void run();
	
	Transmitter *transmitter() const;
	KovanSerial *proto() const;
	
signals:
	void stateChanged(const QString &state);
	void run(const QString &executable);
	
protected:
	bool handle(const Packet &p);
	
private:
	void handleArchive(const Packet &headerPacket);
	void handleAction(const Packet &action);
	
	bool m_stop;
	Transmitter *m_transmitter;
	TransportLayer *m_transport;
	KovanSerial *m_proto;
};

#endif
