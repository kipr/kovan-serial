#include "compile_worker.hpp"

#include <kovanserial/kovan_serial.hpp>
#include <pcompiler/pcompiler.hpp>

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

void CompileWorker::setBinPath(const QString &binPath)
{
	m_binPath = binPath;
}

const QString &CompileWorker::binPath() const
{
	return m_binPath;
}

void CompileWorker::setLibPath(const QString &libPath)
{
	m_libPath = libPath;
}

const QString &CompileWorker::libPath() const
{
	return m_libPath;
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
			QByteArray(), "error: Failed to extract KISS Archive.");
	}
	QStringList extracted;
	foreach(const QString& file, m_archive->files()) extracted << path + "/" + file;
	qDebug() << "Extracted" << extracted;

	// Invoke pcompiler on the extracted files
	Engine engine(Compilers::instance()->compilers());
	Options opts = Options::load("/etc/kovan/platform.hints");
	opts.replace("${PREFIX}", QDir::currentPath() + "/prefix");
	Compiler::OutputList ret = engine.compile(Input::fromList(extracted), opts, this);

	// Pick out successful terminals
	Compiler::OutputList terminals;
	foreach(const Output& out, ret) {
		if(out.isTerminal() && out.generatedFiles().size() == 1) {
			if(out.isSuccess()) terminals << out;
			else qDebug() << "Terminal type" << out.terminal() << "unsuccessful.";
		}
	}
	if(terminals.isEmpty()) {
		ret << Output(path, 1,
			QByteArray(), "Warning: No successful terminals detected from compilation.");
		return ret;
	}

	// Copy terminal files to the appropriate directories
	foreach(const Output& out, terminals) {
		QFileInfo fileInfo(out.generatedFiles()[0]);
		const QString &fullBinPath = (fileInfo.suffix().isEmpty() ? m_binPath : m_binPath + "." + fileInfo.suffix());
		const QString &fullLibPath = (fileInfo.suffix().isEmpty() ? m_libPath : m_libPath + "." + fileInfo.suffix());
		QString destination;
		if(out.terminal() == Output::BinaryTerminal) {
			QFile::remove(fullBinPath);
			destination = fullBinPath;
		}
		else if(out.terminal() == Output::LibraryTerminal) {
			QFile::remove(fullLibPath);
			destination = fullLibPath;
		}
		if(!QFile::copy(fileInfo.absoluteFilePath(), destination)) {
			ret << OutputList() << Output(path, 1, QByteArray(),
				("error: Failed to copy \"" + fileInfo.absoluteFilePath() + "\" to \"" + destination + "\"").toLatin1());
		}
	}

	return ret;
}

QString CompileWorker::tempPath()
{
	return QDir::tempPath() + "/" + QDateTime::currentDateTime().toString("yyMMddhhmmss") + ".kovan-serial";
}
