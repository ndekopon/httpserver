#pragma once

#include "common.hpp"

#include <string>
#include <vector>

namespace app {
	std::wstring s_to_ws(const std::string& _s);
	std::string ws_to_s(const std::wstring& _ws);
}
