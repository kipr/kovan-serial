#include "compile_worker.hpp"
#include "constants.hpp"

#include <kovanserial/kovan_serial.hpp>
#include <pcompiler/pcompiler.hpp>
#include <pcompiler/root_manager.hpp>

#include <QFileInfo>
#include <QDir>
#include <QDateTime>
#include <QDebug>

struct Cleaner
{
public:
	Cleaner(const QString& path)
		: path(path)
	{
	}

	~Cleaner()
	{
		remove(path);
	}

private:
	bool remove(const QString& path)
	{
		QDir dir(path);

		if(!dir.exists()) return true;

		QFileInfoList entries = dir.entryInfoList(QDir::NoDotAndDotDot | QDir::System | QDir::Hidden
			| QDir::AllDirs | QDir::Files, QDir::DirsFirst);

		foreach(const QFileInfo& entry, entries) {
			const QString entryPath = entry.absoluteFilePath();
			if(!(entry.isDir() ? remove(entryPath) : QFile::remove(entryPath))) return false;
		}

		if(!dir.rmdir(path)) return false;

		return true;
	}

	QString path;
};

CompileWorker::CompileWorker(const Kiss::KarPtr &archive, KovanSerial *proto, QObject *parent)
	: QThread(parent),
	m_archive(archive),
	m_proto(proto)
{
}

void CompileWorker::run()
{
	m_output = compile();
	
	qDebug() << "Sending finish!";
	if(!m_proto || !m_proto->sendFileActionProgress(1.0, true)) {
		qWarning() << "send terminal file action progress failed.";
	}
}

const Compiler::OutputList &CompileWorker::output() const
{
	return m_output;
}

void CompileWorker::setName(const QString &name)
{
	m_name = name;
}

const QString &CompileWorker::name() const
{
	return m_name;
}

void CompileWorker::progress(double fraction)
{
	//qDebug() << "Progress..." << fraction;
	if(!m_proto || !m_proto->sendFileActionProgress(false, fraction)) {
		qWarning() << "send file action progress failed.";
	}
}

Compiler::OutputList CompileWorker::compile()
{
	using namespace Compiler;
	using namespace Kiss;

	// Extract the archive to a temporary directory
	QString path = tempPath();
	Cleaner cleaner(path);
	if(!m_archive->extract(path)) {
		return OutputList() << Output(path, 1,
			QByteArray(), "error: failed to extract KISS Archive");
	}
	QStringList extracted;
	foreach(const QString& file, m_archive->files()) extracted << path + "/" + file;
	qDebug() << "Extracted" << extracted;

	// Invoke pcompiler on the extracted files
	Engine engine(Compilers::instance()->compilers());
	Options opts = Options::load("/etc/kovan/platform.hints");
	const QString &includeFlag = QString::fromStdString(" -I" + USER_ROOT + "/include");
	opts["C_FLAGS"] = opts["C_FLAGS"] + includeFlag;
	opts["CPP_FLAGS"] = opts["CPP_FLAGS"] + includeFlag;
	opts.replace("${PREFIX}", QDir::currentPath() + "/prefix");
	Compiler::OutputList ret = engine.compile(Input::fromList(extracted), opts, this);

	// Pick out successful terminals
	Compiler::OutputList terminals;
	foreach(const Output& out, ret) {
		if(out.isTerminal()) {
			if(out.isSuccess()) terminals << out;
			else qDebug() << "Terminal type" << out.terminal() << "unsuccessful.";
		}
	}
	if(terminals.isEmpty()) {
		ret << Output(path, 1, QByteArray(),
			"warning: no successful terminals detected from compilation");
		return ret;
	}

	// Copy terminal files to the appropriate directories
	ret << RootManager::install(terminals, "/kovan/prefix/", m_name);

	return ret;
}

QString CompileWorker::tempPath()
{
	return QDir::tempPath() + "/" + QDateTime::currentDateTime().toString("yyMMddhhmmss") + ".kovan-serial";
}
