#pragma once

#include "common.hpp"

#include <memory>
#include <string>
#include <cstdarg>

namespace app {
	void log_set_window(HWND _window);
	void log(const wchar_t* _str, ...);
	std::unique_ptr<std::wstring> log_read();
}
