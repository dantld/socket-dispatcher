
project(server)

set(CMAKE_CXX_FLAGS ${CMAKE_CXX_FLAGS} -ggdb)

add_executable(mylsnr
	mylsnr.cpp
	utils.cpp
	Socket.h
	Socket.cpp
	SocketsPoller.cpp
	SocketFactory.h
	TcpListenerSocketFactory.cpp
	UnixListenerSocketFactory.cpp
	)

add_executable(mydisp
	mydisp.cpp
	utils.cpp
	Socket.h
	Socket.cpp
	SocketFactory.h
	SocketsPoller.cpp
	ReceivedTcpSocketFactory.cpp
	UnixSocketFactory.cpp
	)
