
project(server)

set(CMAKE_CXX_FLAGS ${CMAKE_CXX_FLAGS} -ggdb)

add_executable(mylsnr
	mylsnr.cpp
	utils.cpp
	Socket.h
	Socket.cpp
	SslSocket.cpp
	SocketUtils.cpp
	SocketsPoller.cpp
	SocketFactory.h
	TcpListenerSocketFactory.cpp
	TcpSslListenerSocketFactory.cpp
	UnixListenerSocketFactory.cpp
	InitSsl.cpp
	ApplicationConfig.cpp
	)

add_executable(mydisp
	mydisp.cpp
	utils.cpp
	Socket.h
	Socket.cpp
	SslSocket.cpp
	SocketUtils.cpp
	SocketFactory.h
	SocketsPoller.cpp
	ReceivedTcpSocketFactory.cpp
	UnixSocketFactory.cpp
	InitSsl.cpp
	ApplicationConfig.cpp
	)

add_executable(myclnt
	myclnt.cpp
	utils.cpp
	Socket.h
	Socket.cpp
	SslSocket.cpp
	SocketUtils.cpp
	SocketFactory.h
	TcpSslConnectSocketFactory.cpp
	SocketsPoller.cpp
	InitSsl.cpp
	ApplicationConfig.cpp
)

target_link_libraries(mylsnr ssl crypto)
target_link_libraries(mydisp ssl crypto)
target_link_libraries(myclnt ssl crypto)
