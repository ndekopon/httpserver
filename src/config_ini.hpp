#pragma once

#include "common.hpp"

#include <string>

namespace app
{

	class config_ini
	{
	private:
		std::wstring path_;

		bool set_value(const std::wstring& _key, const std::wstring& _value);
		std::wstring get_value(const std::wstring& _key);

	public:
		config_ini();
		~config_ini();

		bool set_ipaddress(const std::string &_ipaddress);
		std::string get_ipaddress();

		bool set_port(UINT _port);
		UINT get_port();

		bool set_connections(UINT _connections);
		UINT get_connections();
	};
}
