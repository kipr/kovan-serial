#ifndef _PROPERTIES_HPP_
#define _PROPERTIES_HPP_

#include <QMap>
#include <QMutex>

class QSettings;

class Properties
{
public:
	~Properties();
	
	void setValue(const QString &property, const QString &value);
	QString value(const QString &property) const;
	
	static Properties *instance();
private:
	void sync();
	
	Properties();
	
	QSettings settings() const;
	
	QMap<QString, QString> m_properties;
	QMutex m_mutex;
	QSettings *m_settings;
};

#endif
