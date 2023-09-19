#include "main_window.hpp"

#include "log.hpp"

#include <imm.h>
#include <commctrl.h>

#pragma comment(lib, "imm32.lib")
#pragma comment(lib, "Ws2_32.lib")
#pragma comment(lib, "Comctl32.lib")

namespace
{
}

namespace app
{
	constexpr UINT MID_EDIT_LOG = 1;
	const wchar_t* main_window::window_class_ = L"httpserver-mainwindow";
	const wchar_t* main_window::window_title_ = L"httpserver";
	const wchar_t* main_window::window_mutex_ = L"httpserver_mutex";
	const LONG main_window::window_width_ = 640;
	const LONG main_window::window_height_ = 480;

	main_window::main_window(HINSTANCE _instance)
		: instance_(_instance)
		, window_(nullptr)
		, edit_(nullptr)
		, font_(nullptr)
		, http_thread_()
		, ini_()
	{
		WSADATA wsa;
		::WSAStartup(MAKEWORD(2, 2), &wsa);
	}

	main_window::~main_window()
	{
		::WSACleanup();
	}

	bool main_window::init()
	{
		HANDLE mutex = ::CreateMutexW(NULL, TRUE, window_mutex_);
		if (::GetLastError() == ERROR_ALREADY_EXISTS)
		{
			return false;
		}

		disable_ime();

		set_dpi_awareness();

		// create window
		register_window_class();
		if (!create_window())
			return false;

		return true;
	}


	int main_window::loop()
	{
		MSG message;

		while (::GetMessageW(&message, nullptr, 0, 0))
		{
			::TranslateMessage(&message);
			::DispatchMessageW(&message);
		}
		return (int)message.wParam;
	}

	void main_window::disable_ime()
	{
		::ImmDisableIME(-1);
	}

	void main_window::set_dpi_awareness()
	{
		auto desired_context = DPI_AWARENESS_CONTEXT_UNAWARE_GDISCALED;
		if (::IsValidDpiAwarenessContext(desired_context))
		{
			auto hr = ::SetProcessDpiAwarenessContext(desired_context);
			if (hr)
				return;
		}
	}

	ATOM main_window::register_window_class()
	{
		WNDCLASSEXW wcex;

		wcex.cbSize = sizeof(WNDCLASSEXW);

		wcex.style = CS_HREDRAW | CS_VREDRAW;
		wcex.lpfnWndProc = window_proc_common;
		wcex.cbClsExtra = 0;
		wcex.cbWndExtra = 0;
		wcex.hInstance = instance_;
		wcex.hIcon = ::LoadIconW(nullptr, IDI_APPLICATION);
		wcex.hCursor = ::LoadCursorW(nullptr, IDC_ARROW);
		wcex.hbrBackground = (HBRUSH)(COLOR_WINDOW);
		wcex.lpszMenuName = nullptr;
		wcex.lpszClassName = window_class_;
		wcex.hIconSm = ::LoadIconW(nullptr, IDI_APPLICATION);

		return ::RegisterClassExW(&wcex);
	}

	bool main_window::create_window()
	{
		window_ = ::CreateWindowExW(0, window_class_, window_title_, WS_OVERLAPPEDWINDOW,
			CW_USEDEFAULT, 0, CW_USEDEFAULT, 0, nullptr, nullptr, instance_, this);

		if (window_ == nullptr)
		{
			return false;
		}

		::ShowWindow(window_, SW_NORMAL);
		::UpdateWindow(window_);

		return true;
	}

	HWND main_window::create_edit(HMENU _id, DWORD _x, DWORD _y, DWORD _w, DWORD _h, HFONT _font)
	{
		HWND edit = ::CreateWindowExW(
			0, WC_EDITW, L"",
			WS_CHILD | WS_VISIBLE | WS_BORDER | ES_MULTILINE | WS_VSCROLL | WS_HSCROLL | ES_AUTOHSCROLL | ES_AUTOVSCROLL | ES_READONLY,
			_x, _y, _w, _h, window_, (HMENU)_id, instance_, NULL);
		if (edit)
		{
			::SendMessageW(edit, WM_SETFONT, (WPARAM)_font, MAKELPARAM(1, 0));
			// 文字数制限を取っ払う
			::SendMessageW(edit, EM_SETLIMITTEXT, 0, 0);
		}
		return edit;
	}	

	LRESULT main_window::window_proc(UINT _message, WPARAM _wparam, LPARAM _lparam)
	{
		switch (_message)
		{
		case WM_CREATE:
		{

			// フォント作成
			font_ = ::CreateFontW(
				12, 0, 0, 0, FW_REGULAR,
				FALSE, FALSE, FALSE, DEFAULT_CHARSET, OUT_DEFAULT_PRECIS,
				CLIP_DEFAULT_PRECIS, DEFAULT_QUALITY, DEFAULT_PITCH | FF_MODERN,
				L"MS Shell Dlg");

			{
				RECT rect;
				LONG left = 10;
				::GetClientRect(window_, &rect);

				// エディトボックス
				edit_ = create_edit((HMENU)MID_EDIT_LOG, 10, 10, rect.right - 20, rect.bottom - 20, font_);
			}

			{
				// 読み出し
				auto ip = ini_.get_ipaddress();
				auto port = ini_.get_port();
				auto connections = ini_.get_connections();

				// 書き込み
				ini_.set_ipaddress(ip);
				ini_.set_port(port);
				ini_.set_connections(connections);

				// スレッド開始
				if (!http_thread_.run(window_, ip, port, connections)) return -1;
			}

			// タイマー設定
			::SetTimer(window_, 1, 1000, nullptr);

			return 0;
		}

		case WM_DESTROY:
			// スレッド停止
			http_thread_.stop();

			::DeleteObject(font_);
			::PostQuitMessage(0);
			return 0;

		case WM_COMMAND:
			{
			}
			break;

		case WM_TIMER:
		{
			if (_wparam == 1)
			{
				return 0;
			}
		}
			break;

		case CWM_LOG_UPDATE:
		{
			auto text = log_read();
			if (text)
			{
				auto len = ::SendMessageW(edit_, WM_GETTEXTLENGTH, 0, 0);
				::SendMessageW(edit_, EM_SETSEL, (WPARAM)len, (LPARAM)len);
				::SendMessageW(edit_, EM_REPLACESEL, FALSE, (LPARAM)text->c_str());
			}
		}
			break;

		default:
			break;
		}

		return ::DefWindowProcW(window_, _message, _wparam, _lparam);
	}

	LRESULT CALLBACK main_window::window_proc_common(HWND _window, UINT _message, WPARAM _wparam, LPARAM _lparam)
	{
		if (_message == WM_NCCREATE)
		{
			// createwindowで指定したポイントからインスタンスを取得
			auto cs = reinterpret_cast<CREATESTRUCTW*>(_lparam);
			auto instance = reinterpret_cast<main_window*>(cs->lpCreateParams);

			instance->window_ = _window;

			// ログの設定
			log_set_window(_window);

			// USERDATAにポインタ格納
			::SetWindowLongPtrW(_window, GWLP_USERDATA, reinterpret_cast<LONG_PTR>(instance));
		}
		else if(_message == WM_GETMINMAXINFO)
		{
			MINMAXINFO* mminfo = (MINMAXINFO*)_lparam;
			mminfo->ptMaxSize.x = window_width_;
			mminfo->ptMaxSize.y = window_height_;
			mminfo->ptMaxPosition.x = 0;
			mminfo->ptMaxPosition.y = 0;
			mminfo->ptMinTrackSize.x = window_width_;
			mminfo->ptMinTrackSize.y = window_height_;
			mminfo->ptMaxTrackSize.x = window_width_;
			mminfo->ptMaxTrackSize.y = window_height_;
			return 0;
		}

		// 既にデータが格納されていたらインスタンスのプロシージャを呼び出す
		if (auto ptr = reinterpret_cast<main_window*>(::GetWindowLongPtrW(_window, GWLP_USERDATA)))
		{
			return ptr->window_proc(_message, _wparam, _lparam);
		}

		return ::DefWindowProcW(_window, _message, _wparam, _lparam);
	}
}
