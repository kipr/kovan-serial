#ifndef _SERIAL_BRIDGE_HPP_
#define _SERIAL_BRIDGE_HPP_

#include <QObject>

class SerialBridge : public QObject
{
Q_OBJECT
public slots:
  bool run(const QString &path);
};

#endif