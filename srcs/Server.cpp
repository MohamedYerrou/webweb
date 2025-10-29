#include "../includes/Server.hpp"
#include "../includes/Server.hpp"
#include <sys/socket.h>
#include "../includes/Utils.hpp"
#include "../includes/Utils.hpp"
#include <iostream>
#include <string.h>
#include <sys/epoll.h>
#include <arpa/inet.h>
#include <cerrno>
#include <utility>
#include <utility>

Server::Server()
{
	client_max_body_size = 1024;
}

Server::~Server()
{
}

//getters

const std::vector<Location>& Server::getLocations() const
{
	return locations;
}

void    Server::push_listen(std::pair<std::string, int> pair)
{
    listens.push_back(pair);
}

void    Server::push_location(Location location)
{
    locations.push_back(location);
}

size_t							Server::getMaxSize() const
{
	return client_max_body_size;
}

void    Server::init_server(int epfd, std::map<int, Server*>& server_fd)
{
    for (size_t i = 0; i < listens.size(); i++)
	{
		int listen_fd = socket(AF_INET, SOCK_STREAM, 0);
		if (listen_fd == -1)
			throw_exception("socket: ", strerror(errno));

		int opt = 1;
		if (setsockopt(listen_fd, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt)) == -1)
			throw_exception("setsockopt: ", strerror(errno));

		// server_fd.push_back(listen_fd);
		server_fd.insert(std::make_pair(listen_fd, this));

		sockaddr_in addr;

		memset(&addr, 0, sizeof(addr));
		addr.sin_family = AF_INET;
		// std::cout << listens[i].second << std::endl;
		addr.sin_port = htons(listens[i].second);
		// std::cout << addr.sin_port << std::endl;
		addr.sin_addr.s_addr = inet_addr(listens[i].first.c_str());
		// std::cout << listens[i].first << std::endl;

		if (bind(listen_fd, (const sockaddr*)&addr, sizeof(addr)) == -1)
			throw_exception("bind: ", strerror(errno));

		if (listen(listen_fd, SOMAXCONN) == -1)
			throw_exception("listen: ", strerror(errno));

		setNonBlocking(listen_fd);

		std::cout << "Listening on: " << listens[i].first << ":" <<
		listens[i].second << std::endl;

		struct epoll_event ev;
		ev.events = EPOLLIN;
		ev.data.fd = listen_fd;

		if (epoll_ctl(epfd, EPOLL_CTL_ADD, listen_fd, &ev) == -1)
			throw_exception("epoll_ctl: ", strerror(errno));
	}
}


void	Server::addCgiIn(CGIContext CGIctx, int epfd)
{
	int pipe_fd = CGIctx.pipe_to_cgi;
		
	if (CGIstdIn.find(pipe_fd) != CGIstdIn.end())
		throw_exception("addCgiIn: ", "CGI input pipe already registered");
	
	CGIstdIn[pipe_fd] = CGIctx;
	
	setNonBlocking(pipe_fd);
	
	struct epoll_event ev;
	ev.events = EPOLLOUT | EPOLLERR | EPOLLHUP;
	ev.data.fd = pipe_fd;
	
	if (epoll_ctl(epfd, EPOLL_CTL_ADD, pipe_fd, &ev) == -1)
		throw_exception("epoll_ctl: ", strerror(errno));
}


void	Server::addCgiOut(CGIContext CGIctx, int epfd)
{
	int pipe_fd = CGIctx.pipe_from_cgi;
		
	if (CGIstdOut.find(pipe_fd) != CGIstdOut.end())
		throw_exception("addCgiOut: ", "CGI output pipe already registered");
	
	CGIstdOut[pipe_fd] = CGIctx;
	
	setNonBlocking(pipe_fd);
	
	struct epoll_event ev;
	ev.events = EPOLLIN | EPOLLERR | EPOLLHUP;
	ev.data.fd = pipe_fd;
	
	if (epoll_ctl(epfd, EPOLL_CTL_ADD, pipe_fd, &ev) == -1)
		throw_exception("epoll_ctl: ", strerror(errno));
}