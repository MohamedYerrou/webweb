#ifndef REQUEST_HPP
#define REQUEST_HPP

#include <iostream>
#include <string>
#include <map>
#include <sstream>
#include <cstdlib>
#include <vector>
#include <cctype>
#include <fcntl.h>
#include <string.h>
#include <unistd.h>
#include <cerrno>
#include <cstdio>
#include "Utils.hpp"

class Request
{
	private:
		int									uploadFile;
		std::string							method;
		std::string							uri;
		std::string							path;
		std::string							protocol;
		std::map<std::string, std::string>	headers;
		std::map<std::string, std::string>	queries;
		size_t								MAX_URL_LENGTH;
		int									statusCode;
		std::string							body;



	public:
		Request();
		~Request();
		int 										getStatusCode();
		const std::string&							getMethod() const;
		const std::string&							getUri() const;
		const std::string&							getPath() const;
		const std::string&							getProtocol() const;
		const std::map<std::string, std::string>&	getHeaders() const;
		const std::map<std::string, std::string>&	getQueries() const;
		void										parseRequest(const std::string& raw);
		void										parseLine(const std::string& raw);
		void										parseHeaders(const std::string& raw);
		size_t										getContentLength() const;
		bool										parseMethod(const std::string& method);
		bool										parseUri(const std::string& uri);
		std::string									normalizePath(const std::string& path);
		std::string									decodeUri(const std::string& uri, bool isQuery);
		std::string									removeFragment(const std::string& uri);
		void										splitUri(const std::string& str);
		void										parseQuery(const std::string& query);
		void										appendBody(const char* buf, size_t length);
		void										generateTmpFile(const std::string& target_path, const std::string& file);
		void										closeFileUpload();

		const std::string& 							getBody() const;
};

#endif