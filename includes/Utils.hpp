#ifndef UTILS_HPP
#define UTILS_HPP

#include <string>
#include <vector>
#include "Server.hpp"
#include <sstream>
#include <sys/stat.h>
#include <dirent.h>

class Request;

class MyException: public std::exception
{
	private:
		std::string msg;
	public:
		MyException(std::string msg): msg(msg){};
		const char* what() const throw() {return msg.c_str();};
		virtual ~MyException() _GLIBCXX_TXN_SAFE_DYN _GLIBCXX_NOTHROW {};
};

// void		run_server(int epfd, std::map<int, Server*>& servers_fd);
bool		listening_fd(std::map<int, Server*>& servers_fd, int fd);
void		throw_exception(std::string function, std::string err);
void		setNonBlocking(int fd);
std::string	intTostring(size_t value);
std::string	getStatusText(int code);
std::string	currentDate();
std::string	trim(std::string str);
int			stringToInt(const std::string& str, int base = 10);
void		parsedRequest(Request req);
bool		isFile(const std::string& path);
bool		isDir(const std::string& path);
std::string getMimeType(const std::string& path);
bool    allowedDelete(std::string path);
bool    isEmpty(const std::string& path);
size_t  getFileSize(const std::string& path);
void    toLowerCase(std::string& str);

#endif