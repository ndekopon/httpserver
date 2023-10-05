#include "http_server.hpp"

#include "log.hpp"

#include "utils.hpp"

#include <Ws2tcpip.h>
#include <mswsock.h>

#pragma comment(lib, "Ws2_32.lib")
#pragma comment(lib, "Mswsock.lib")

namespace {
	constexpr auto HTTP_BUFFER_SIZE = 16 * 1024; // 16KB
	constexpr auto FILE_BUFFER_SIZE = 64 * 1024; // 64KB
}

namespace app {

	std::wstring get_remote_ipport(LPVOID _buffer, DWORD _len)
	{
		wchar_t s[INET_ADDRSTRLEN] = {L'\0'};
		DWORD slen = INET_ADDRSTRLEN;


		SOCKADDR_IN* l;
		SOCKADDR_IN* r;
		INT llen = sizeof(SOCKADDR_IN);
		INT rlen = sizeof(SOCKADDR_IN);
		::GetAcceptExSockaddrs(_buffer, _len, llen + 16, rlen + 16, reinterpret_cast<sockaddr **>(&l), &llen, reinterpret_cast<sockaddr**>(&r), &rlen);

		::WSAAddressToStringW((SOCKADDR *)r, rlen, NULL, s, &slen);
		return s;
		;

	}

	http_server::http_server(const std::string address, uint16_t port, uint16_t maxconn)
		: listen_address_(address)
		, listen_port_(port)
		, addr_buffer_()
		, accept_ctx_()
		, conns_(maxconn)
		, sock_(INVALID_SOCKET)
	{
		// 初期化
		for (auto& x : conns_)
		{
			x.ior_ctx.buf.resize(HTTP_BUFFER_SIZE);
			x.ior_ctx.wsabuf.buf = reinterpret_cast<CHAR*>(x.ior_ctx.buf.data());
			x.ior_ctx.wsabuf.len = x.ior_ctx.buf.size();
			x.ior_ctx.type = HTTP_TCP_RECV;

			x.iow_ctx.buf.resize(HTTP_BUFFER_SIZE);
			x.iow_ctx.wsabuf.buf = reinterpret_cast<CHAR*>(x.iow_ctx.buf.data());
			x.iow_ctx.wsabuf.len = 0;
			x.iow_ctx.type = HTTP_TCP_SEND;

			x.fio_ctx.buf.at(0).resize(FILE_BUFFER_SIZE);
			x.fio_ctx.buf.at(1).resize(FILE_BUFFER_SIZE);
		}
	}

	http_server::~http_server()
	{
		// 各接続の切断
		for (auto& x : conns_)
		{
			if (x.sock != INVALID_SOCKET) connection_close(&x);
		}

		// Listenポートを閉じる
		if (sock_ != INVALID_SOCKET)
		{
			::closesocket(sock_);
		}
	}

	bool http_server::tcp_socket()
	{
		sock_ = ::WSASocketW(AF_INET, SOCK_STREAM, IPPROTO_IP, NULL, 0, WSA_FLAG_OVERLAPPED);
		if (sock_ == INVALID_SOCKET)
		{
			log(L"Error: WSASocket() failed. WSAGetLstError()=%d", ::WSAGetLastError());
			return false;
		}
		return true;
	}

	bool http_server::tcp_bind()
	{
		struct sockaddr_in addr;
		addr.sin_family = AF_INET;
		addr.sin_port = htons(listen_port_);
		inet_pton(AF_INET, listen_address_.c_str(), &addr.sin_addr.s_addr);
		if (::bind(sock_, (struct sockaddr*)&addr, sizeof(addr)) != 0)
		{
			log(L"Error: bind() failed. WSAGetLastError()=%d", ::WSAGetLastError());
			return false;
		}
		return true;
	}

	bool http_server::tcp_listen()
	{
		if (::listen(sock_, 16) != 0)
		{
			log(L"Error: listen() failed. WSAGetLastError()=%d", ::WSAGetLastError());
			return false;
		}
		return true;
	}

	http_conn_t* http_server::insert(SOCKET _sock)
	{
		for (auto& x : conns_)
		{
			if (x.sock == INVALID_SOCKET)
			{
				x.sock = _sock;
				return &x;
			}
		}
		return nullptr;
	}

	bool http_server::prepare()
	{
		if (!tcp_socket()) return false;
		if (!tcp_bind()) return false;
		if (!tcp_listen()) return false;
		log(L"Info: listen websocket server at %s:%d", s_to_ws(listen_address_).c_str(), listen_port_);
		return true;
	}

	size_t http_server::count() const noexcept
	{
		size_t i = 0;
		for (const auto& x : conns_)
			if (x.sock != INVALID_SOCKET) i++;
		return i;
	}

	bool http_server::tcp_send(http_conn_t* _conn, const std::vector<char*>& _data)
	{
		if (_conn->sock == INVALID_SOCKET) return false;

		HTTP_IO_CONTEXT& ctx = _conn->iow_ctx;

		// バッファコピー
		if (_data.size() > ctx.buf.size()) ctx.buf.resize(_data.size());
		std::memcpy(ctx.buf.data(), _data.data(), _data.size());

		// バッファに情報を格納
		std::memset(&ctx.ov, 0, sizeof(WSAOVERLAPPED));
		ctx.wsabuf.buf = ctx.buf.data();
		ctx.wsabuf.len = _data.size();
		auto rc = ::WSASend(_conn->sock, &ctx.wsabuf, 1, nullptr, 0, &ctx.ov, nullptr);
		if (rc == 0)
		{
			return true;
		}
		
		auto error = ::WSAGetLastError();
		if (error != ERROR_IO_PENDING)
		{
			log(L"Error: WSASend() failed. ErrorCode=%d", error);
			connection_close(_conn);
			return false;
		}

		return true;
	}

	bool http_server::tcp_send(http_conn_t* _conn, const std::string& _str)
	{
		if (_conn->sock == INVALID_SOCKET) return false;

		HTTP_IO_CONTEXT& ctx = _conn->iow_ctx;

		// バッファコピー
		if (_str.size() > ctx.buf.size()) ctx.buf.resize(_str.size());
		std::copy(_str.begin(), _str.end(), ctx.buf.begin());

		// バッファに情報を格納
		std::memset(&ctx.ov, 0, sizeof(WSAOVERLAPPED));
		ctx.wsabuf.buf = ctx.buf.data();
		ctx.wsabuf.len = _str.size();
		auto rc = ::WSASend(_conn->sock, &ctx.wsabuf, 1, nullptr, 0, &ctx.ov, nullptr);
		if (rc == 0)
		{
			return true;
		}

		auto error = ::WSAGetLastError();
		if (error != ERROR_IO_PENDING)
		{
			log(L"Error: WSASend() failed. ErrorCode=%d", error);
			connection_close(_conn);
			return false;
		}


		return true;
	}

	bool http_server::tcp_send_file(http_conn_t* _conn)
	{
		if (_conn->sock == INVALID_SOCKET) return false;

		HTTP_IO_CONTEXT& ctx = _conn->iow_ctx;
		FILE_IO_CONTEXT& fctx = _conn->fio_ctx;
		size_t index = (fctx.sent_count % 2);
		std::vector<char>& buf = fctx.buf.at(index);
		DWORD transferred = fctx.transferred.at(index);

		// バッファに情報を格納
		std::memset(&ctx.ov, 0, sizeof(WSAOVERLAPPED));
		ctx.wsabuf.buf = buf.data();
		ctx.wsabuf.len = transferred;
		auto rc = ::WSASend(_conn->sock, &ctx.wsabuf, 1, nullptr, 0, &ctx.ov, nullptr);
		if (rc == 0)
		{
			fctx.sending = true;
			return true;
		}

		auto error = ::WSAGetLastError();
		if (error != ERROR_IO_PENDING)
		{
			log(L"Error: WSASend() failed. ErrorCode=%d", error);
			connection_close(_conn);
			return false;
		}

		fctx.sending = true;
		return true;
	}

	bool http_server::tcp_read(http_conn_t *_conn)
	{
		if (_conn->sock == INVALID_SOCKET) return false;

		HTTP_IO_CONTEXT& ctx = _conn->ior_ctx;

		// バッファに情報を格納
		std::memset(&ctx.ov, 0, sizeof(WSAOVERLAPPED));

		DWORD flags = 0;
		auto rc = ::WSARecv(_conn->sock, &ctx.wsabuf, 1, NULL, &flags, &ctx.ov, NULL);
		if (rc == 0)
		{
			return true;
		}
		
		auto error = ::WSAGetLastError();
		if (error != WSA_IO_PENDING)
		{
			log(L"Error: WSARecv() failed. WSAGetLastError()=%d", error);
			connection_close(_conn);
			return false;
		}

		return true;
	}

	bool http_server::tcp_acceptex()
	{
		accept_ctx_.sock = ::WSASocketW(AF_INET, SOCK_STREAM, IPPROTO_IP, NULL, 0, WSA_FLAG_OVERLAPPED);
		accept_ctx_.data = addr_buffer_.data();

		auto rc = ::AcceptEx(sock_, accept_ctx_.sock, addr_buffer_.data(), 0,
			sizeof(SOCKADDR_IN) + 16, sizeof(SOCKADDR_IN) + 16, NULL, &accept_ctx_.ov);

		if (rc == TRUE)
		{
			return true;
		}

		auto error = ::WSAGetLastError();
		if (error != ERROR_IO_PENDING)
		{
			log(L"Error: AcceptEx() failed. WSAGetLastError()=", error);
			return false;
		}

		return true;
	}

	bool http_server::file_open(http_conn_t* _conn, const std::wstring& _path)
	{
		FILE_IO_CONTEXT& ctx = _conn->fio_ctx;
		if (ctx.file != INVALID_HANDLE_VALUE)
		{
			return false;
		}

		ctx.file = ::CreateFileW(_path.c_str(), GENERIC_READ, FILE_SHARE_READ | FILE_SHARE_WRITE, NULL, OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL | FILE_FLAG_OVERLAPPED, NULL);
		if (ctx.file == INVALID_HANDLE_VALUE)
		{
			return false;
		}
		log(L"Info: sock=%d open file handle = %p", _conn->sock, ctx.file);

		std::memset(&ctx.ov, 0, sizeof(OVERLAPPED));

		LARGE_INTEGER size;
		if (!::GetFileSizeEx(ctx.file, &size))
		{
			file_close(_conn);
			return false;
		}
		ctx.size = size.QuadPart;
		log(L"Info: sock=%d open file size = %llu", _conn->sock, ctx.size);

		ctx.read_count = 0;
		ctx.sent_count = 0;
		ctx.total_read = 0;
		ctx.total_sent = 0;
		ctx.sending = false;
		ctx.reading = false;

		return true;
	}

	bool http_server::file_read(http_conn_t* _conn)
	{
		FILE_IO_CONTEXT& ctx = _conn->fio_ctx;
		if (ctx.file == INVALID_HANDLE_VALUE)
		{
			return false;
		}

		if (::ReadFile(ctx.file, ctx.buf.at(ctx.read_count % 2).data(), ctx.buf.at(ctx.read_count % 2).size(), NULL, &ctx.ov))
		{
			ctx.reading = true;
			return true;
		}

		auto error = ::GetLastError();
		if (error != ERROR_IO_PENDING)
		{
			log(L"Error: ReadFile() failed. GetLastError()=", error);
			return false;
		}

		ctx.reading = true;
		return true;
	}

	void http_server::file_close(http_conn_t* _conn)
	{
		FILE_IO_CONTEXT& ctx = _conn->fio_ctx;

		if (ctx.file != INVALID_HANDLE_VALUE)
		{
			::CancelIo(ctx.file);
			::CloseHandle(ctx.file);
			log(L"Info: sock=%d close file handle = %p", _conn->sock, ctx.file);
		}
		ctx.file = INVALID_HANDLE_VALUE;
	}

	void http_server::connection_close(http_conn_t *_conn)
	{
		file_close(_conn);

		if (_conn->sock != INVALID_SOCKET)
		{
			log(L"Info: close socket = %d", _conn->sock);
			::closesocket(_conn->sock);
			_conn->sock = INVALID_SOCKET;
		}
	}
}
