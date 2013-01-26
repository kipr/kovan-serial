#include "server_thread.hpp"

#include "compile_worker.hpp"
#include "constants.hpp"

#include <kovanserial/transmitter.hpp>
#include <kovanserial/kovan_serial.hpp>
#include <kovanserial/command_types.hpp>
#include <kovanserial/general.hpp>
#include <kovanserial/platform_defines.hpp>

#include <QDebug>
#include <QFileInfo>
#include <QDir>

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
	unsigned long i = 1;
	while(!m_stop) {
		QThread::msleep(100);
		Transmitter::Return ret = m_transport->recv(p, 2);
		if(ret == Transmitter::Success && handle(p)) std::cout << "Finished handling one command" << std::endl;
		
		// Linux will report an EIO error if the usb device is in an error state.
		// The only problem is that we have to *write* to get that error code.
		// This writes an array of size zero every two seconds to check for EIO.
		if(i++ % 20 == 0) {
			uint8_t dummy[0];
			if(m_transmitter->write(dummy, 0) >= 0) continue;
			qDebug() << "USB ERROR!!!";
			// USB has entered error state.
			m_transmitter->endSession();
			m_transmitter->makeAvailable();
		}
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
	if(p.type == Command::KnockKnock) m_proto->whosThere();
	else if(p.type == Command::FileHeader) handleArchive(p);
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
	
	// Remove old binary
	remove((USER_BINARIES_DIR + KOVAN_SERIAL_PATH_SEP + header.dest).c_str());
	
	std::ofstream file((USER_ARCHIVES_DIR + KOVAN_SERIAL_PATH_SEP
		+ header.dest).c_str(), std::ios::binary);
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
	
	const QString type = data.action;
	std::cout << "Handling acton: " << data.action << std::endl;
	
	if(type == COMMAND_ACTION_READ) {
		QFileInfo info(data.dest);
		if(info.isDir()) {
			std::stringstream stream;
			const bool good = info.exists();
			if(!m_proto->confirmFileAction(good) || !good) return;
			QList<QFileInfo> entries = info.dir().entryInfoList(QDir::NoDot |
				QDir::NoDotDot | QDir::Dirs | QDir::Files);
			foreach(const QFileInfo &entry, entries) {
				char typeChar = 0;
				if(entry.isDir()) typeChar = 'd';
				else if(entry.isFile()) typeChar = 'f';
				else if(entry.isSymLink()) typeChar = 'l';
				else typeChar = '?';
				
				stream << typeChar << " " << entry.fileName().toStdString() << std::endl;
			}
			stream.seekg(0, std::ios_base::beg);
			if(!m_proto->sendFile(data.dest, "", &stream)) {
				std::cout << "Sending results failed." << std::endl;
			}
			return;
		}
		std::ifstream file(data.dest, std::ios::binary);
		const bool good = file.is_open();

		if(!m_proto->confirmFileAction(good) || !good) {
			std::cout << "Confirm failed with " << good << std::endl;
			return;
		}
		
		if(!m_proto->sendFile(data.dest, "", &file)) {
			std::cout << "Sending results failed." << std::endl;
		}
		file.close();
		return;
	}
	
	if(type == COMMAND_ACTION_SCREENSHOT) {
		system("cat /dev/fb0 > /latest_screenshot.raw565");
		
		std::ifstream file("/latest_screenshot.raw565", std::ios::binary);
		const bool good = file.is_open();
		if(!m_proto->confirmFileAction(good) || !good) {
			std::cout << "Confirm failed with " << good << std::endl;
			return;
		}
		if(!m_proto->sendFile("/latest_screenshot.raw565", "", &file)) {
			std::cout << "Sending results failed." << std::endl;
		}
		file.close();
		std::cout << "Action screenshot finished" << std::endl;
		return;
	}

	const QString arcPath = QString::fromStdString(USER_ARCHIVES_DIR) + "/" + data.dest;
	const QString binPath = QString::fromStdString(USER_BINARIES_DIR) + "/" + data.dest;
	
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
	} else m_proto->confirmFileAction(false);
}
