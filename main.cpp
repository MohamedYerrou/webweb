#include <fstream>
#include <iostream>
#include <vector>
#include <sstream>
#include <string>
#include <utility> 
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <sys/epoll.h>
#include <fcntl.h>
#include <string.h>
#include <unistd.h>
#include <stdlib.h>
#include <stdio.h>
#include <map>
#include "includes/Request.hpp"
#include "includes/Location.hpp"
#include "includes/Client.hpp"
#include "includes/Server.hpp"
#include "includes/Utils.hpp"
#include <cstdio>


void	setNonBlocking(int fd) {
    int flags = fcntl(fd, F_GETFL, 0);
    if (fcntl(fd, F_SETFL, flags | O_NONBLOCK) == -1)
		throw_exception("fcntl: ", strerror(errno));
}

void	tokenizer(std::vector<std::string>&	tokens, std::string configFile)
{
	std::fstream				file(configFile.c_str());
	std::string					line;
	std::string					word;
	std::stringstream			ss;

	while (std::getline(file, line))
	{
		std::stringstream	ss(line);
		if (!line.empty() && line[0] == '#')
			continue;
		while (ss >> word)
		{
			if (!line.empty() && word[0] == '#')
				break;
			if (!word.empty() && word[word.size() - 1] == ';')
			{
				word.erase(word.size() - 1);
				tokens.push_back(word);
				tokens.push_back(";");
			}
			else if (!word.empty() && word[word.size() - 1] != ';')
				tokens.push_back(word);
		}
	}
}

std::vector<Server> parser(std::vector<std::string>& tokens)
{
	std::vector<Server>	config;
	Location			currLocation;
	Server				currServer;
	std::string			tok;
	bool				inLocation = false;
	bool				inServer = false;


	for (size_t i = 0; i < tokens.size(); i++)
	{
		tok = tokens[i];
		if (tok == "server")
		{
			currServer = Server();
			inServer = true;
			i++;
		}
		else if (tok == "listen" && inServer)
		{
			int							port;
			std::string					ip;
			std::pair<std::string, int> p;


			tok = tokens[++i];
			ip = tok.substr(0, tok.find(':'));
			port = atoi(tok.substr(tok.find(':') + 1).c_str());
			p = make_pair(ip, port);
			currServer.push_listen(p);
			i++;
		}
		else if (tok == "location" && inServer && !inLocation)
		{
			inLocation = true;
			currLocation = Location();
			currLocation.set_path(tokens[++i]);
		}
		else if (tok == "error_page" && inServer && inLocation)
		{
			int			code;
			std::string	path;

			code = atoi(tokens[++i].c_str());
			path = tokens[++i];
			currLocation.insert_error(make_pair(code, path));
		}
		else if (tok == "}" && inLocation)
		{
			currServer.push_location(currLocation);
			inLocation = false;
		}
		else if (tok == "root" && inLocation)
			currLocation.set_root(tokens[++i]);
		else if (tok == "index" && inLocation)
		{
			while (tokens[++i] != ";")
				currLocation.push_index(tokens[i]);
		}
		else if (tok == "upload_store" && inLocation)
			currLocation.set_upload_store(tokens[++i]);
		else if (tok == "methods" && inLocation)
		{
			i++;
			while (tokens[i] == "GET" || tokens[i] == "POST" || tokens[i] == "DELETE")
				currLocation.push_method(tokens[i++]);
		}
		else if (tok == "cgi" && inLocation)
		{
			std::string	ext = tokens[++i];
			std::string path = tokens[++i];
			currLocation.insert_cgi(make_pair(ext, path));
		}
		else if (tok == "return" && inLocation)
		{
			std::string	path;
			int			status;
			
			status = atoi(tokens[++i].c_str());
			path = tokens[++i];
			currLocation.set_redir(make_pair(status, path));
			currLocation.set_isRedirection(true);
		}
		else if (tok == "autoindex")
		{
			if (tokens[++i] == "on")
				currLocation.set_autoIndex(true);
		}
		else if (tok == "}" && inServer && !inLocation)
		{
			config.push_back(currServer);
			inServer = false;
		}
	}
	return config;
}

bool listening_fd(std::map<int, Server*>& servers_fd, int fd)
{
	std::map<int, Server*>::iterator it = servers_fd.find(fd);
	if (it != servers_fd.end())
		return true;
	return false;
}

void	throw_exception(std::string function, std::string err)
{
	throw MyException(function + err);
}

int main(int ac, char** av)
{
	std::vector<std::string>	tokens;
	std::vector<Server>			servers;
	std::map<int, Server*>		servers_fd;

	try
	{
		tokenizer(tokens, (ac != 2 ? "config_files/configFile1.txt" : av[1]));
		servers = parser(tokens);
		int epfd = epoll_create(1);
		for (size_t i = 0; i < servers.size(); ++i) {
            servers[i].init_server(epfd, servers_fd);
        }
		Server::run_server(epfd, servers_fd);
	}
	catch(const std::exception& e)
	{
		std::cerr << e.what() << '\n';
		exit(1);
	}
    return 0;
}

