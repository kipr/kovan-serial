#include "server_thread.hpp"

#include "compile_worker.hpp"
#include "constants.hpp"

#include <kovanserial/transmitter.hpp>
#include <kovanserial/kovan_serial.hpp>
#include <kovanserial/command_types.hpp>
#include <kovanserial/general.hpp>
#include <kovanserial/platform_defines.hpp>

#include <QDebug>
#include <QFile>

#include <fstream>
#include <iostream>
#include <sstream>

ServerThread::ServerThread(Transmitter *transmitter)
	: m_stop(false),
	m_transmitter(transmitter),
	m_transport(new TransportLayer(m_transmitter)),
	m_proto(new KovanSerial(m_transport))
{
}

ServerThread::~ServerThread()
{
	delete m_proto;
	delete m_transport;
	delete m_transmitter;
}

void ServerThread::stop()
{
	m_stop = true;
}

bool ServerThread::isStopping() const
{
	return m_stop;
}

void ServerThread::run()
{
	Packet p;
	while(!m_stop) {
		while(m_proto->next(p, 5000) && handle(p));
		QThread::yieldCurrentThread();
	}
}

Transmitter *ServerThread::transmitter() const
{
	return m_transmitter;
}

KovanSerial *ServerThread::proto() const
{
	return m_proto;
}

bool ServerThread::handle(const Packet &p)
{
	qDebug() << "Got packet of type" << p.type;
	if(p.type == Command::FileHeader) handleArchive(p);
	else if(p.type == Command::FileAction) handleAction(p);
	else if(p.type == Command::Hangup) return false;
	return true;
}

void ServerThread::handleArchive(const Packet &headerPacket)
{
	quint64 start = msystime();
	
	Command::FileHeaderData header;
	headerPacket.as(header);
	bool good = QString(header.metadata) == "kar";
	if(!good) {
		m_proto->confirmFile(false);
		return;
	}
	
	std::ofstream file((USER_ARCHIVES_DIR + KOVAN_SERIAL_PATH_SEP + header.dest).c_str(), std::ios::binary);
	good = file.is_open();
	if(!m_proto->confirmFile(good) || !good) return;
	
	if(!m_proto->recvFile(header.size, &file, 1000)) {
		qWarning() << "recvFile failed";
		return;
	}
	
	file.close();
	
	quint64 end = msystime();
	qDebug() << "Took" << (end - start) << "milliseconds to recv";
}

void ServerThread::handleAction(const Packet &action)
{
	Command::FileActionData data;
	action.as(data);

	const QString arcPath = QString::fromStdString(USER_ARCHIVES_DIR) + "/" + data.dest;
	const QString binPath = QString::fromStdString(USER_BINARIES_DIR) + "/" + data.dest;
	
	QString type = data.action;
	if(type == COMMAND_ACTION_COMPILE) {
		Kiss::KarPtr archive = Kiss::Kar::load(arcPath);
		const bool good = !archive.isNull();
		qDebug() << "good?" << good;
		if(!m_proto->confirmFileAction(good) || !good) return;
		
		CompileWorker *worker = new CompileWorker(archive, m_proto);
		worker->setResultPath(binPath);
		worker->start();
		worker->wait();
		
		qDebug() << "Sending results...";
		QByteArray data;
		QDataStream stream(&data, QIODevice::WriteOnly);
		stream << worker->output();
		
		std::istringstream sstream;
		sstream.rdbuf()->pubsetbuf(data.data(), data.size());
		if(!m_proto->sendFile("", "col", &sstream)) {
			qWarning() << "Sending result failed";
			return;
		}
	} else if(type == COMMAND_ACTION_RUN) {
		const bool good = QFile::exists(binPath);
		qDebug() << "good?" << good;
		if(!m_proto->confirmFileAction(good) || !good) return;
		m_proto->sendFileActionProgress(true, 1.0);
		emit run(binPath);
	}
}
