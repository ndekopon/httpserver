#include "http_thread.hpp"

#include "log.hpp"

#include "http_server.hpp"
#include "utils.hpp"

#include <tuple>
#include <unordered_map>

namespace {

	const std::array<bool, 0x80> http_available_ascii_codes =
	{
		0, 0, 0, 0, 0, 0, 0, 0,
		0, 1, 1, 0, 0, 1, 0, 0, // \t, \n, \r
		0, 0, 0, 0, 0, 0, 0, 0,
		0, 0, 0, 0, 0, 0, 0, 0,
		1, 1, 1, 1, 1, 1, 1, 1,
		1, 1, 1, 1, 1, 1, 1, 1,
		1, 1, 1, 1, 1, 1, 1, 1,
		1, 1, 1, 1, 1, 1, 1, 1,
		1, 1, 1, 1, 1, 1, 1, 1,
		1, 1, 1, 1, 1, 1, 1, 1,
		1, 1, 1, 1, 1, 1, 1, 1,
		1, 1, 1, 1, 1, 1, 1, 1,
		1, 1, 1, 1, 1, 1, 1, 1,
		1, 1, 1, 1, 1, 1, 1, 1,
		1, 1, 1, 1, 1, 1, 1, 1,
		1, 1, 1, 1, 1, 1, 1, 0
	};

	const std::array<bool, 0x80> absolute_path_codes =
	{
		0, 0, 0, 0, 0, 0, 0, 0,
		0, 0, 0, 0, 0, 0, 0, 0,
		0, 0, 0, 0, 0, 0, 0, 0,
		0, 0, 0, 0, 0, 0, 0, 0,
		0, 0, 0, 0, 0, 0, 0, 0,
		0, 0, 0, 0, 0, 1, 1, 1, // 0x2D(-) 0x2E(.) 0x2F(/)
		1, 1, 1, 1, 1, 1, 1, 1, // 0x30-
		1, 1, 0, 0, 0, 0, 0, 0, // -0x39 数字
		1, 1, 1, 1, 1, 1, 1, 1, // 0x40(@) 0x41-
		1, 1, 1, 1, 1, 1, 1, 1, // -
		1, 1, 1, 1, 1, 1, 1, 1, // -
		1, 1, 1, 0, 0, 0, 0, 1, // -0x5A ALPHA 0x5F(_)
		0, 1, 1, 1, 1, 1, 1, 1, // 0x61-
		1, 1, 1, 1, 1, 1, 1, 1, // -
		1, 1, 1, 1, 1, 1, 1, 1, // -
		1, 1, 1, 0, 0, 0, 1, 0  // -0x7A alpha 0x7E(~)
	};

	std::unordered_map<std::string, std::string> ext_map = {
		{"css", "text/css"},
		{"csv", "text/csv"},
		{"txt", "text/plain"},
		{"vtt", "text/vtt"},
		{"html", "text/html"},
		{"htm", "text/html"},
		{"apng", "image/apng"},
		{"avif", "image/avif"},
		{"bmp", "image/bmp"},
		{"gif", "image/gif"},
		{"png", "image/png"},
		{"svg", "image/svg+xml"},
		{"webp", "image/webp"},
		{"ico", "image/x-icon"},
		{"tif", "image/tiff"},
		{"tiff", "image/tiff"},
		{"jpeg", "image/jpeg"},
		{"jpg", "image/jpeg"},
		{"mp4", "video/mp4"},
		{"mpeg", "video/mpeg"},
		{"webm", "video/webm"},
		{"mp3", "audio/mp3"},
		{"mpga", "audio/mpeg"},
		{"weba", "audio/webm"},
		{"wav", "audio/wave"},
		{"otf", "font/otf"},
		{"ttf", "font/ttf"},
		{"woff", "font/woff"},
		{"woff2", "font/woff2"},
		{"7z", "application/x-7z-compressed"},
		{"atom", "application/atom+xml"},
		{"pdf", "application/pdf"},
		{"mjs", "application/javascript"},
		{"js", "application/javascript"},
		{"json", "application/json"},
		{"rss", "application/rss+xml"},
		{"tar", "application/x-tar"},
		{"xhtml", "application/xhtml+xml"},
		{"xht", "application/xhtml+xml"},
		{"xslt", "application/xslt+xml"},
		{"xml", "application/xml"},
		{"gz", "application/gzip"},
		{"zip", "application/zip"},
		{"wasm", "application/wasm"}
	};

	inline std::string trim(const std::string& s)
	{
		auto a = s.find_first_not_of(" \t\r\n");
		if (a == std::string::npos) return "";
		auto b = s.find_last_not_of(" \t\r\n");
		return s.substr(a, b - a + 1);
	}

	bool check_ascii(const std::vector<char>& _s, size_t _size)
	{
		for (size_t i = 0; i < _size; ++i)
		{
			auto c = _s.at(i);
			if (c > 0x7f || !http_available_ascii_codes.at(c))
			{
				app::log(L"Error: Invalid char is %d.", (DWORD)c);
				return false;
			}
		}
		return true;
	}

	std::tuple<int, std::string, std::string, std::string, std::unordered_map<std::string, std::string>>
		parse_http_header(const std::vector<char>& _header, size_t _size)
	{
		enum : int {
			SEC_METHOD,
			SEC_REQUEST,
			SEC_VERSION,
			SEC_KEY,
			SEC_VALUE
		};

		int sec = SEC_METHOD;
		int rc = -1;
		char prev_c = 0;

		bool invalid_char = false;
		std::string method = "";
		bool method_oversize = false;
		std::string request = "";
		bool request_oversize = false;
		std::string version = "";
		bool version_oversize = false;
		std::string key = "";
		std::string value = "";
		bool header_end = false;
		bool invalid_keyvalue = false;
		std::unordered_map<std::string, std::string> key_values;

		// サイズ予約
		request.reserve(_size);
		key.reserve(_size);
		value.reserve(_size);

		for (size_t i = 0; i < _header.size() && i < _size; ++i)
		{
			char c = _header.at(i);

			// check valid char
			if (c > 0x7f || !http_available_ascii_codes.at(c))
			{
				invalid_char = true;
			}
			else
			{

				switch (sec)
				{
				case SEC_METHOD:
					if (c == ' ')
					{
						sec = SEC_REQUEST;
					}
					else
					{
						if (method.size() > 7)
						{
							// CONNECT/OPTIONS = 7chars
							method_oversize = true;
						}
						else
						{
							method += c;
						}
					}
					break;
				case SEC_REQUEST:
					if (c == ' ')
					{
						sec = SEC_VERSION;
					}
					else
					{
						if (request.size() > 4096)
						{
							request_oversize = true;
						}
						else
						{
							request += c;
						}
					}
					break;
				case SEC_VERSION:
					if (c == '\n' && prev_c == '\r')
					{
						sec = SEC_KEY;
						version.pop_back(); // 末尾の\rを削除
					}
					else
					{
						if (version.size() > 8)
						{
							// HTTP/1.1 = 8chars
							version_oversize = true;
						}
						else
						{
							version += c;
						}
					}
					break;
				case SEC_KEY:
					if (c == '\n' && prev_c == '\r')
					{
						key.pop_back(); // 末尾の\rを削除

						if (key == "")
						{
							header_end = true;
						}
						else
						{
							invalid_keyvalue = true;
						}
					}
					else if (c == ':')
					{
						sec = SEC_VALUE;
					}
					else
					{
						key += c;
					}
					break;
				case SEC_VALUE:
					if (c == '\n' && prev_c == '\r')
					{
						sec = SEC_KEY;
						value.pop_back(); // 末尾の\rを削除

						// key valueの格納
						const auto trimed_key = trim(key);
						const auto trimed_value = trim(value);
						if (trimed_key == "" || trimed_value == "")
						{
							invalid_keyvalue = true;
						}
						else
						{
							if (key_values.contains(trimed_key))
							{
								key_values.at(trimed_key) += (", " + trimed_value);
							}
							else
							{
								key_values.insert({ trimed_key, trimed_value });
							}
							key = "";
							value = "";
						}
					}
					else
					{
						value = value += c;
					}
					break;
				}
			}

			prev_c = c;

			if (invalid_char) break;
			if (method_oversize) break;
			if (request_oversize) break;
			if (version_oversize) break;
			if (invalid_keyvalue) break;
			if (header_end) break;
		}

		// 解析終了後のチェック
		if (invalid_char) rc = -1;
		else if (method_oversize) rc = -2;
		else if (request_oversize) rc = -3;
		else if (version_oversize) rc = -4;
		else if (invalid_keyvalue) rc = -5;
		else if (!header_end) rc = -6;
		else rc = 0;

		return {
			rc,
			method,
			request,
			version,
			key_values
		};
	}

	std::string get_absolute_path(const std::string& _request)
	{
		std::string r = "";
		r.reserve(_request.size());
		char prev_c = 0;

		for (size_t i = 0; i < _request.size(); ++i)
		{
			char c = _request.at(i);
			if (r == "" && c != '/') // スラッシュで始まらないURL禁止
				return "";

			if (c == '/' && prev_c == '/') // 連続スラッシュ禁止
				return "";
			if (c == '/' && prev_c == '.') // .で終わるフォルダ禁止
				return "";
			if (c == '.' && prev_c == '/') // .で始まるファイル/フォルダは禁止
				return "";
			if (c == '.' && prev_c == '.') // 連続ドット禁止
				return "";

			if (c == '?') // クエリ以降は無視
				break;

			if (absolute_path_codes[c] == 0) // 許可されていない文字が含まれていた
				return "";

			r += c;

			prev_c = c;
		}

		if (r.back() == '.') // .で終わるファイルは禁止
			return "";

		return r;
	}

	std::wstring get_htdocs()
	{
		std::vector<WCHAR> buf(32767, L'\0');
		std::wstring r = L"";

		// モジュールインスタンスからDLLパスを取得
		auto loaded = ::GetModuleFileNameW(::GetModuleHandleW(nullptr), buf.data(), buf.size());

		for (DWORD i = loaded - 1; i != 0; --i)
		{
			if (buf.at(i) == L'\\')
			{
				buf.at(i) = L'\0';
				break;
			}
		}

		r.reserve(loaded + 10);

		// パスの合成
		if (buf.at(0) == L'\\' && buf.at(1) == L'\\')
		{
			r += L"\\\\?\\UN";
			buf.at(0) = L'C';
		}
		else
		{
			r += L"\\\\?\\";
		}
		r += buf.data();
		r += L"\\htdocs";

		app::log(L"Info: htdocs = %s", r.c_str());

		return r;
	}

	std::wstring absolute_path_to_winpath(const std::string& _path)
	{
		std::wstring r = app::s_to_ws(_path);
		for (auto& c : r)
		{
			if (c == L'/') c = L'\\';
		}
		return r;
	}

	bool is_file(const std::wstring& _path)
	{
		auto attr = ::GetFileAttributesW(_path.c_str());
		return ((attr != INVALID_FILE_ATTRIBUTES) && !(attr & FILE_ATTRIBUTE_DIRECTORY));
	}

	std::string get_content_type(const std::wstring& _path)
	{
		bool has_ext = false;
		std::string ext = "";
		ext.reserve(255);
		for (size_t i = _path.size() - 1; i != 0; --i)
		{
			char c = (_path.at(i) & 0xff);
			if (c == '\\')
			{
				has_ext = false;
				break;
			}
			else if (c == '.')
			{
				has_ext = true;
				break;
			}
			ext = c + ext;
		}

		if (!has_ext || ext == "")
			return "application/octet-stream";
		
		if (ext_map.contains(ext))
			return ext_map.at(ext);

		return "application/octet-stream";
	}
}


namespace app {
	http_thread::http_thread()
		: ip_("127.0.0.1")
		, port_(20082)
		, maxconn_(16)
		, window_(NULL)
		, thread_(NULL)
		, compport_(NULL)
	{
	}

	http_thread::~http_thread()
	{
		stop();
	}

	DWORD WINAPI http_thread::proc_common(LPVOID _p)
	{
		auto p = reinterpret_cast<http_thread*>(_p);
		return p->proc();
	}

	DWORD http_thread::proc()
	{
		log(L"Info: thread start.");

		std::wstring htdocs_path = get_htdocs();
		if (::CreateDirectoryW(htdocs_path.c_str(), NULL))
		{
			app::log(L"Info: htdocs directory created.");
		}

		http_server server(ip_.c_str(), port_, maxconn_);
		if (server.prepare())
		{
			log(L"Info: http_server::prepare() success.");

			auto port = ::CreateIoCompletionPort((HANDLE)server.sock_, compport_, COMPKEY_TCP_ACCEPTEX, 0);

			// 接続待ち
			if (!server.tcp_acceptex())
			{
				log(L"Error: http_server::acceptex() failed.");
				return 0;
			}

			while (true)
			{
				DWORD transferred;
				ULONG_PTR compkey;
				LPOVERLAPPED ov;
				auto rc = ::GetQueuedCompletionStatus(compport_, &transferred, &compkey, &ov, INFINITE);
				if (rc == FALSE) continue;

				if (compkey == COMPKEY_OPERATION && ov == NULL)
				{
					if (transferred == OPERATION_STOP)
					{
						// 終了通知
						break;
					}
					else if (transferred == OPERATION_KEEPALIVE_CHECK)
					{
						// KEEPALIVE切断処理
					}
				}

				if (compkey == COMPKEY_TCP_ACCEPTEX && ov != NULL)
				{
					// ACCEPT
					HTTP_ACCEPT_CONTEXT *ctx = (HTTP_ACCEPT_CONTEXT*)ov;
					SOCKET sock = ctx->sock;
					auto ipport = get_remote_ipport(ctx->data, transferred);
					log(L"Info: connected from %s.", ipport.c_str());

					if (!server.tcp_acceptex())
					{
						log(L"Error: http_server::tcp_acceptex() failed.");
						break;
					}

					auto conn = server.insert(sock);
					if (conn == nullptr)
					{
						log(L"Error: reached max connection.");
						server.connection_close(conn);
						continue;
					}

					// 接続元の表示
					log(L"Info: ACCEPT called. sock=%d.", conn->sock);

					::CreateIoCompletionPort((HANDLE)conn->sock, compport_, COMPKEY_TCP_READWRITE, 0);

					// 読込待ち
					if (!server.tcp_read(conn))
					{
						log(L"Error: websocket_server::read() failed.");
					}
				}
				if (compkey == COMPKEY_TCP_READWRITE && ov != NULL)
				{
					HTTP_IO_CONTEXT* ctx = (HTTP_IO_CONTEXT*)ov;
					http_conn_t* conn = ctx->conn;

					if (transferred == 0 && (ctx->type == HTTP_TCP_RECV || ctx->type == HTTP_TCP_SEND))
					{
						// IO完了かつ転送バイト0は終了
						server.connection_close(ctx->conn);
					}
					else if (ctx->type == HTTP_TCP_RECV)
					{
						// データ受信完了
						const auto &[rc, method, request, version, kvs] = parse_http_header(ctx->buf, transferred);
						
						if (rc < 0)
						{
							// HTTPプロトコルを話していない
							server.connection_close(conn);
						}
						else
						{
							// ログに表示
							log(L"Info: sock=%d << %s %s %s", conn->sock, s_to_ws(method).c_str(), s_to_ws(request).c_str(), s_to_ws(version).c_str());
							for (auto const& kv : kvs)
							{
								log(L"Info: sock=%d << %s: %s", conn->sock, s_to_ws(kv.first).c_str(), s_to_ws(kv.second).c_str());
							}

							// ヘッダ未返送
							conn->headersent = false;

							// keep-aliveチェック
							if (kvs.contains("Connection") && kvs.at("Connection") == "close")
							{
								conn->keepalive = false;
							}
							else
							{
								conn->keepalive = true;
							}

							if (version != "HTTP/1.1")
							{
								const std::string res = 
									version + " 505 HTTP Version Not Supported\r\n"
									"X-Server-Message: Only support HTTP/1.1.\r\n"
									"Content-Length: 0\r\n"
									"Connection: close\r\n"
									"\r\n";
								conn->keepalive = false;
								if (!server.tcp_send(conn, res))
								{
									log(L"Error: http_sever::send() failed.");
									server.connection_close(conn);
								}
							}
							else if (method != "GET" && method != "HEAD")
							{
								const std::string res =
									"HTTP/1.1 405 Method Not Allowed\r\n"
									"Allow: GET, HEAD\r\n"
									"Content-Length: 0\r\n"
									"Connection: close\r\n"
									"\r\n";
								conn->keepalive = false;
								if (!server.tcp_send(conn, res))
								{
									log(L"Error: http_sever::tcp_send() failed.");
									server.connection_close(conn);
								}
							}
							else
							{
								std::string res = "";
								auto absolutepath = get_absolute_path(request);
								if (absolutepath == "")
								{
									res =
										"HTTP/1.1 400 Bad Request\r\n"
										"Content-Length: 0\r\n"
										"\r\n";
								}
								else
								{
									// スラッシュで終わってたらindex.html表示を試みる
									if (absolutepath.back() == '/')
									{
										absolutepath += "index.html";
									}
									auto path = htdocs_path + absolute_path_to_winpath(absolutepath);
									auto exists = is_file(path);
									log(L"Info: sock=%d absolutepath=%s", conn->sock, s_to_ws(absolutepath).c_str());
									log(L"Info: sock=%d path=%s", conn->sock, path.c_str());
									log(L"Info: sock=%d exists=%s", conn->sock, exists ? L"true" : L"false");

									if (exists)
									{
										if (server.file_open(conn, path))
										{
											::CreateIoCompletionPort(conn->fio_ctx.file, compport_, COMPKEY_FILE_READ, 0);

											bool ok = true;

											if (method == "GET" && conn->fio_ctx.size > 0)
											{
												if (!server.file_read(conn))
												{
													log(L"Error: http_sever::file_read() failed.");
													server.file_close(conn);
													ok = false;
												}
												else
												{
													log(L"Info: http_sever::file_read() start.");
													conn->fio_ctx.sending = true;
												}
											}
											else if (conn->fio_ctx.size == 0)
											{
												server.file_close(conn);
											}

											if (ok)
											{
												res = "HTTP/1.1 200 OK\r\n";
												res += "Content-Type: " + get_content_type(path) + "\r\n";
												res += "Content-Length: " + std::to_string(conn->fio_ctx.size) + "\r\n";
												res += "\r\n";
											}

											if (method == "HEAD")
											{
												conn->fio_ctx.size = 0;
												server.file_close(conn);
											}
										}
									}

									if (res == "")
									{
										res =
											"HTTP/1.1 404 Not Found\r\n"
											"Content-Length: 0\r\n"
											"\r\n";
									}
								}
								if (!server.tcp_send(conn, res))
								{
									log(L"Error: http_sever::tcp_send() failed.");
									server.connection_close(conn);
								}
							}

							// 読込待ち
							if (!server.tcp_read(conn))
							{
								log(L"Error: http_server::tcp_read() failed.");
								server.connection_close(conn);
							}
						}
					}
					else if (ctx->type == HTTP_TCP_SEND)
					{
						// データ書き込み完了
						if (conn->headersent == false)
						{
							conn->headersent = true;
						}
						else
						{
							conn->fio_ctx.sent_count++;
							conn->fio_ctx.total_sent += transferred;
						}
						conn->fio_ctx.sending = false;

						// log(L"Info: file send total=%llu current=%llu count=%llu.", conn->fio_ctx.size, conn->fio_ctx.total_sent, conn->fio_ctx.sent_count);

						// 読込バッファがたまっている
						if (conn->fio_ctx.sent_count < conn->fio_ctx.read_count)
						{
							if (!server.tcp_send_file(conn))
							{
								log(L"Error: http_server::tcp_send_file() failed.");
								server.connection_close(conn);
							}
						}

						// ファイル読込が送信完了待ちしてた
						if (!conn->fio_ctx.reading && conn->fio_ctx.size > conn->fio_ctx.total_read)
						{
							if (conn->fio_ctx.sent_count < conn->fio_ctx.read_count)
							{
								if (!server.file_read(conn))
								{
									log(L"Error: http_server::file_read() failed.");
									server.connection_close(conn);
								}
							}
						}

						// 切断処理
						if (conn->fio_ctx.size <= conn->fio_ctx.sent_count)
						{
							if (!conn->keepalive)
							{
								server.connection_close(conn);
							}
						}
					}
				}
				if (compkey == COMPKEY_FILE_READ && ov != NULL)
				{
					FILE_IO_CONTEXT* ctx = (FILE_IO_CONTEXT*)ov;
					http_conn_t* conn = ctx->conn;

					const DWORD read_index = (ctx->read_count % 2);
					ctx->transferred.at(read_index) = transferred;
					ctx->total_read += transferred;
					ctx->read_count++;
					ctx->reading = false;
					ctx->ov.Offset = ctx->total_read & 0xffffffff;
					ctx->ov.OffsetHigh = (ctx->total_read >> 32) & 0xffffffff;

					// log(L"Info: file read total=%llu current=%llu count=%llu.", ctx->size, ctx->total_read, ctx->read_count);

					if (ctx->read_count <= ctx->sent_count + 2)
					{
						// 次のファイル読込
						if (!server.file_read(conn))
						{
							log(L"Error: http_server::file_read() failed.");
							server.connection_close(conn);
						}
					}

					// ファイル読込待ち
					if (!ctx->sending && ctx->size > ctx->total_sent)
					{
						if (ctx->sent_count < ctx->read_count)
						{
							if (!server.tcp_send_file(conn))
							{
								log(L"Error: http_server::tcp_send_file() failed.");
								server.connection_close(conn);
							}
						}
					}

					// 読込が完了した
					if (ctx->size == ctx->total_read)
					{
						log(L"Info: sock=%d file read complete.", conn->sock);
						server.file_close(conn);
					}
				}
			}
		}
		log(L"Info: thread end.");

		return 0;
	}

	bool http_thread::run(HWND _window, const std::string& _ip, uint16_t _port, uint16_t _maxconn)
	{
		window_ = _window;
		ip_ = _ip;
		port_ = _port;
		maxconn_ = _maxconn;

		// CompPort作成
		compport_ = ::CreateIoCompletionPort(INVALID_HANDLE_VALUE, NULL, COMPKEY_OPERATION, 0);
		if (compport_ == NULL)
		{
			log(L"Error: CreateIoCompletionPort() failed.");
			return false;
		}

		// スレッド起動
		thread_ = ::CreateThread(NULL, 0, proc_common, this, 0, NULL);
		return thread_ != NULL;
	}

	void http_thread::stop()
	{
		if (thread_ != NULL)
		{
			::PostQueuedCompletionStatus(compport_, OPERATION_STOP, COMPKEY_OPERATION, NULL);
			::WaitForSingleObject(thread_, INFINITE);
			thread_ = NULL;
		}

		if (compport_ != NULL)
		{
			::CloseHandle(compport_);
			compport_ = NULL;
		}
	}
}
