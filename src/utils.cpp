#include "utils.hpp"

namespace app {
	std::wstring s_to_ws(const std::string& _s)
	{
		auto ilen = _s.length();
		auto olen = ::MultiByteToWideChar(CP_UTF8, 0, _s.c_str(), ilen, 0, 0);
		std::vector<wchar_t> r(olen + 1, L'\0');
		if (olen)
			::MultiByteToWideChar(CP_UTF8, 0, _s.c_str(), ilen, r.data(), olen);
		return r.data();
	}

	std::string ws_to_s(const std::wstring& _ws)
	{
		auto ilen = _ws.length();
		auto olen = ::WideCharToMultiByte(CP_UTF8, 0, _ws.c_str(), ilen, NULL, 0, NULL, FALSE);
		std::vector<char> r(olen + 1, L'\0');
		if (olen)
			::WideCharToMultiByte(CP_UTF8, 0, _ws.c_str(), ilen, r.data(), olen, NULL, FALSE);
		return r.data();
	}
}
