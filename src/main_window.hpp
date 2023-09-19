#pragma once

#include "common.hpp"

#include "config_ini.hpp"
#include "http_thread.hpp"

namespace app
{
	class main_window
	{
	private:
		const HINSTANCE instance_;
		HWND window_;
		HWND edit_;
		HFONT font_;
		config_ini ini_;
		http_thread http_thread_;

		static const wchar_t* window_class_;
		static const wchar_t* window_title_;
		static const wchar_t* window_mutex_;
		static const LONG window_width_;
		static const LONG window_height_;

		void disable_ime();
		void set_dpi_awareness();
		ATOM register_window_class();
		bool create_window();
		HWND create_edit(HMENU, DWORD, DWORD, DWORD, DWORD, HFONT);

		LRESULT window_proc(UINT, WPARAM, LPARAM);
		static LRESULT CALLBACK window_proc_common(HWND, UINT, WPARAM, LPARAM);

	public:
		main_window(HINSTANCE);
		~main_window();

		// コピー不可
		main_window(const main_window&) = delete;
		main_window& operator = (const main_window&) = delete;
		// ムーブ不可
		main_window(main_window&&) = delete;
		main_window& operator = (main_window&&) = delete;

		bool init();
		int  loop();
	};
}
