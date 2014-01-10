#include "serial_bridge.hpp"

#include <QLocalSocket>

bool SerialBridge::run(const QString &path)
{
  QLocalSocket socket;
  socket.connectToServer("org.kipr.botui.Run");
  if(!socket.waitForConnected(2000)) return false;
  socket.write(path.toUtf8());
  if(!socket.waitForReadyRead(2000)) return false;
  const bool ret = socket.readAll()[0];
  socket.disconnectFromServer();
  return ret;
}