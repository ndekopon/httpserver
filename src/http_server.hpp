#pragma once

#include "common.hpp"

#include <array>
#include <vector>
#include <string>
#include <cstdint>

namespace app {

	constexpr UINT HTTP_TCP_RECV = 1001;
	constexpr UINT HTTP_TCP_SEND = 1002;
	constexpr UINT HTTP_FILE_READ = 1003;

	struct HTTP_ACCEPT_CONTEXT {
		WSAOVERLAPPED ov;
		SOCKET sock;
		char* data;
	};

	struct http_conn_t;

	struct HTTP_IO_CONTEXT {
		WSAOVERLAPPED ov;
		UINT type;
		WSABUF wsabuf;
		std::vector<char> buf;
		http_conn_t* conn;
	};

	struct FILE_IO_CONTEXT {
		OVERLAPPED ov;
		HANDLE file;
		uint64_t size;
		uint64_t sent_count;
		uint64_t read_count;
		uint64_t total_read;
		uint64_t total_sent;
		bool sending;
		bool reading;
		std::array<std::vector<char>, 2> buf;
		std::array<DWORD, 2> transferred;
		http_conn_t* conn;
	};

	std::wstring get_remote_ipport(LPVOID _buffer, DWORD _len);

	struct http_conn_t {
		SOCKET sock;
		HTTP_IO_CONTEXT ior_ctx;
		HTTP_IO_CONTEXT iow_ctx;
		FILE_IO_CONTEXT fio_ctx;
		std::string path;
		bool headersent;
		bool keepalive;

		http_conn_t() : sock(INVALID_SOCKET), ior_ctx(), iow_ctx(), fio_ctx(), path(), headersent(false), keepalive(false)
		{
			ior_ctx.conn = this;
			iow_ctx.conn = this;
			fio_ctx.conn = this;
			fio_ctx.file = INVALID_HANDLE_VALUE;
		}
	};

	class http_server {
	private:
		std::string listen_address_;
		uint16_t listen_port_;
		std::array<char, 1024> addr_buffer_;
		HTTP_ACCEPT_CONTEXT accept_ctx_;
		std::vector<http_conn_t> conns_;


		bool tcp_socket();
		bool tcp_bind();
		bool tcp_listen();

	public:
		SOCKET sock_;

		http_server(const std::string _address, uint16_t _port, uint16_t _maxconn);
		~http_server();

		bool prepare();

		size_t count() const noexcept;

		bool tcp_acceptex();
		bool tcp_read(http_conn_t* _conn);
		bool tcp_send_file(http_conn_t* _conn);
		bool tcp_send(http_conn_t* _conn, const std::vector<char *>& _data);
		bool tcp_send(http_conn_t* _conn, const std::string& _data);

		bool file_open(http_conn_t* _conn, const std::wstring &_path);
		bool file_read(http_conn_t* _conn);

		http_conn_t *insert(SOCKET _sock);
		void file_close(http_conn_t* _conn);
		void connection_close(http_conn_t* _conn);
	};
}
