#pragma once

#include "common.hpp"

#include <string>

namespace app
{
	constexpr ULONG_PTR COMPKEY_OPERATION = 0;
	constexpr ULONG_PTR COMPKEY_TCP_ACCEPTEX = 1;
	constexpr ULONG_PTR COMPKEY_TCP_READWRITE = 2;
	constexpr ULONG_PTR COMPKEY_FILE_READ = 3;
	constexpr DWORD OPERATION_STOP = 0;
	constexpr DWORD OPERATION_KEEPALIVE_CHECK = 1;




	class http_thread
	{
	private:
		std::string ip_;
		uint16_t port_;
		uint16_t maxconn_;
		HWND window_;
		HANDLE thread_;
		HANDLE compport_;

		static DWORD WINAPI proc_common(LPVOID);
		DWORD proc();
	public:
		http_thread();
		~http_thread();

		// コピー不可
		http_thread(const http_thread&) = delete;
		http_thread& operator = (const http_thread&) = delete;
		// ムーブ不可
		http_thread(http_thread&&) = delete;
		http_thread& operator = (http_thread&&) = delete;

		bool run(HWND, const std::string& _ip, uint16_t _port, uint16_t _maxconn);
		void stop();
	};
}
