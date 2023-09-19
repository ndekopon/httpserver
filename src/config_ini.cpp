#include "config_ini.hpp"

#include "utils.hpp"

#include <vector>

namespace {
	std::wstring get_ini_path()
	{
		std::vector<WCHAR> buf(32767, L'\0');

		// モジュールインスタンスからDLLパスを取得
		auto loaded = ::GetModuleFileNameW(::GetModuleHandleW(nullptr), buf.data(), buf.size());
		auto error = ::GetLastError();

		for (DWORD i = loaded - 1; i != 0; --i)
		{
			if (buf.at(i) == L'\\')
			{
				buf.at(i) = L'\0';
				break;
			}
		}

		// パスの合成
		std::wstring r = L"";
		r.reserve(loaded + 10);
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
		r += L"\\httpserver.ini";
		return r;
	}

	std::wstring uint_to_ws(UINT _u)
	{
		wchar_t table[] = { L'0', L'1', L'2', L'3', L'4', L'5', L'6', L'7', L'8', L'9' };
		std::wstring ws = L"";
		while (true)
		{
			UINT i = (_u % 10);
			ws = table[i] + ws;
			_u = (_u / 10);
			if (_u == 0) break;
		}
		return ws;
	}
}


namespace app {

	const WCHAR section_name[] = L"MAIN";

	config_ini::config_ini()
		: path_(get_ini_path())
	{
	}

	config_ini::~config_ini()
	{
	}

	bool config_ini::set_value(const std::wstring& _key, const std::wstring& _value)
	{
		auto r = ::WritePrivateProfileStringW(section_name, _key.c_str(), _value.c_str(), path_.c_str());
		return r == TRUE;
	}

	std::wstring config_ini::get_value(const std::wstring& _key)
	{
		std::vector<WCHAR> buffer(32767, L'\0');
		auto readed = ::GetPrivateProfileStringW(section_name, _key.c_str(), L"", buffer.data(), buffer.size(), path_.c_str());
		return buffer.data();
	}

	bool config_ini::set_ipaddress(const std::string& _ip)
	{
		auto wip = s_to_ws(_ip);
		return set_value(L"IP", wip);
	}
	
	std::string config_ini::get_ipaddress()
	{
		auto value = ws_to_s(get_value(L"IP"));
		if (value == "")
		{
			return "127.0.0.1";
		}
		return value;
	}

	bool config_ini::set_port(UINT _port)
	{
		return set_value(L"PORT", uint_to_ws(_port));
	}

	UINT config_ini::get_port()
	{
		return ::GetPrivateProfileIntW(section_name, L"PORT", 20082, path_.c_str());
	}

	bool config_ini::set_connections(UINT _connections)
	{
		return set_value(L"CONNECTIONS", uint_to_ws(_connections));
	}

	UINT config_ini::get_connections()
	{
		return ::GetPrivateProfileIntW(section_name, L"CONNECTIONS", 64, path_.c_str());
	}
}
