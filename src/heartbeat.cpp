#include "heartbeat.hpp"

#include <kovan/config.hpp>

#include <QTimer>
#include <QDebug>

#define DEVICE_SETTINGS "/etc/kovan/device.conf"

Heartbeat::Heartbeat(QObject *parent)
	: QObject(parent),
	m_advertiser(true)
{
	QTimer *timer = new QTimer(this);
	connect(timer, SIGNAL(timeout()), SLOT(beat()));
	timer->start(3000);
	
	QTimer *timer2 = new QTimer(this);
	connect(timer2, SIGNAL(timeout()), SLOT(reset()));
	timer2->start(10000);
}

Heartbeat::~Heartbeat()
{
	
}

void Heartbeat::setAdvert(const Advert &advert)
{
	m_advert = advert;
}

const Advert &Heartbeat::advert() const
{
	return m_advert;
}

void Heartbeat::reset()
{

}

void Heartbeat::beat()
{
	// TODO: We really, really should cache this and listen for changes with inotify
	Config *settings = Config::load(DEVICE_SETTINGS);
	QString name = tr("Nameless");
	if(settings) name = QString::fromStdString(settings->stringValue("device_name"));
	m_advert = Advert("Unknown", "Unknown", "KIPR Link", name.toUtf8());
	delete settings;
	m_advertiser.reset();
	m_advertiser.pulse(m_advert);
	qDebug() << "Beat with" << name;
}