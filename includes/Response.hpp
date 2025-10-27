#ifndef RESPONSE_HPP
#define RESPONSE_HPP

#include <string>
#include <iostream>
#include <sstream>
#include <map>
#include "Utils.hpp"

class Response
{
	private:
		std::string							_protocol;
		int									codeStatus;
		std::string							textStatus;
		std::map<std::string, std::string>	headers;
		std::string							body;
		bool								sentHeaders;
	public:
		Response();
		~Response();
		void		setProtocol(std::string protocol);
		void		setStatus(int code, std::string text);
		void		setHeaders(std::string key, std::string value);
		void		setBody(std::string body);
		std::string	build();
		void	setEndHeaders(bool flag);
};

#endif