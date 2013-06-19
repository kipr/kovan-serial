#ifndef _COMPILE_WORKER_HPP_
#define _COMPILE_WORKER_HPP_

#include <QThread>

#include <kar.hpp>
#include <pcompiler/output.hpp>
#include <pcompiler/progress.hpp>

class KovanSerial;

class CompileWorker : public QThread, public Compiler::Progress
{
public:
	CompileWorker(const Kiss::KarPtr &archive, KovanSerial *proto, QObject *parent = 0);
	
	void run();
	
	const Compiler::OutputList &output() const;
	
	void setBinPath(const QString &binPath);
	void setLibPath(const QString &libPath);
	void setHeadPath(const QString &headPath);

	const QString &binPath() const;
	const QString &libPath() const;
	const QString &headPath() const;
	
	void progress(double fraction);
	
private:
	Compiler::OutputList compile();
	static QString tempPath();
	
	Kiss::KarPtr m_archive;
	KovanSerial *m_proto;
	Compiler::OutputList m_output;
	QString m_binPath;
	QString m_libPath;
	QString m_headPath;
};

#endif
