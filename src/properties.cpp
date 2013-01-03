#include "properties.hpp"

#include <QStringList>
#include <QSettings>

Properties::~Properties()
{
	delete m_settings;
}

void Properties::setValue(const QString &property, const QString &value)
{
	m_properties[property] = value;
	sync();
}

QString Properties::value(const QString &property) const
{
	return m_properties.value(property);
}
	
Properties *Properties::instance()
{
	static Properties s_instance;
	return &s_instance;
}

void Properties::sync()
{
	foreach(const QString &key, m_properties.keys()) {
		m_settings->setValue(key, m_properties[key]);
	}
	m_settings->sync();
}

Properties::Properties()
	: m_settings(new QSettings("/kovan/.serial_properties", QSettings::IniFormat))
{	
	foreach(const QString &key, m_settings->allKeys()) {
		m_properties[key] = m_settings->value(key).toString();
	}
}

