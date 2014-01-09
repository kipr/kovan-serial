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
	foreach(const QString &file, m_archive->files()) extracted << path
		+ "/" + file;
	qDebug() << "Extracted" << extracted;

	// Invoke pcompiler on the extracted files
	Engine engine(Compilers::instance()->compilers());
	Options opts = Options::load("/etc/kovan/platform.hints");
	opts.setVariable("${USER_ROOT}", USER_ROOT);
	
	Compiler::OutputList ret = engine.compile(Input::fromList(extracted), opts, this);

	// Pick out successful terminals
	bool terminal = false;
	foreach(const Output &out, ret) {
		if(!out.isTerminal() || !out.isSuccess() || out.generatedFiles().isEmpty()) continue;
		terminal = true;
		
		const Output::TerminalType type = out.terminal();
		if(type == Output::BinaryTerminal) {
			ret << Output(out.generatedFiles()[0], 0, "note: successfully generated executable",
				QByteArray());
		} else if(type == Output::LibraryTerminal) {
			ret << Output(out.generatedFiles()[0], 0, "note: successfully generated library",
				QByteArray());
		}
	}
	
	if(!terminal) return ret;
	
	// Copy terminal files to the appropriate directories
	ret << RootManager(USER_ROOT).install(terminals, ret);
	return ret;
}

QString CompileWorker::tempPath()
{
	return QDir::tempPath() + "/" + QDateTime::currentDateTime().toString("yyMMddhhmmss")
		+ ".kovan-serial";
}
