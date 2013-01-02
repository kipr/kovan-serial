#ifndef _TCP_SERVER_THREAD_HPP_
#define _TCP_SERVER_THREAD_HPP_

#include "server_thread.hpp"

class TcpServer;

class TcpServerThread : public ServerThread
{
public:
	TcpServerThread(TcpServer *transmitter);
	
	virtual void run();
};

#endif
