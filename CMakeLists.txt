cmake_minimum_required(VERSION 3.0)

project(server)

set(CMAKE_CXX_FLAGS_DEBUG "-Wall -Wpedantic -Werror -ggdb")
set(CMAKE_CXX_FLAGS_RELEASE "-Wall -Wpedantic -Werror")
set(CMAKE_CXX_FLAGS_SANITIZE "-Wall -Wpedantic -Werror -ggdb -fsanitize=address -fno-omit-frame-pointer -fsanitize=undefined")

find_package(OpenSSL REQUIRED)

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

target_link_libraries(mylsnr OpenSSL::SSL OpenSSL::Crypto)
target_link_libraries(mydisp OpenSSL::SSL OpenSSL::Crypto)
target_link_libraries(myclnt OpenSSL::SSL OpenSSL::Crypto)
