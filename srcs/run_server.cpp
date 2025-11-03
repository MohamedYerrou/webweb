#include <fstream>
#include <sstream>
#include <utility> 
#include <vector>
#include <string>
#include <iostream>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <sys/epoll.h>
#include <fcntl.h>
#include <string.h>
#include <unistd.h>
#include <stdlib.h>
#include <stdio.h>
#include <csignal>
#include "../includes/Server.hpp"
#include "../includes/Client.hpp"
#include "../includes/Utils.hpp"
#include "../includes/Server.hpp"
#include "../includes/Client.hpp"
#include "../includes/Utils.hpp"

void	handleListeningClient(int epfd, int fd, std::map<int, Client*>& clients, std::map<int, Server*>& servers_fd)
{
	int client_fd = accept(fd, NULL, NULL);
	if (client_fd == -1)
		throw_exception("accept: ", strerror(errno));
	setNonBlocking(client_fd);
	struct epoll_event ev;
	ev.events = EPOLLIN;
	ev.data.fd = client_fd;
	if (epoll_ctl(epfd, EPOLL_CTL_ADD, client_fd, &ev) == -1)
		throw_exception("epoll_ctl: ", strerror(errno));

	clients[client_fd] = new Client(client_fd, servers_fd.find(fd)->second);
}

void	handleClientRequest(int epfd, int fd, std::map<int, Client*>& clients)
{
	char	buf[3000];
	Client* clientPtr = clients[fd];

	ssize_t received = recv(clientPtr->getFD(), buf, sizeof(buf) - 1, 0);
	if (received == -1)
	{
		std::cout << "Recv error: " << strerror(errno) << std::endl;
		epoll_ctl(epfd, EPOLL_CTL_DEL, fd, NULL);
		delete clientPtr;
		clients.erase(fd);
		close(fd);
	}
	else if (received == 0)
	{
		epoll_ctl(epfd, EPOLL_CTL_DEL, fd, NULL);
		delete clientPtr;
		clients.erase(fd);
		close(fd);
	}
	else
	{
		buf[received] = '\0';
		clientPtr->appendData(buf, received);
		//step6: if request complete
		if (clientPtr->getReqComplete())
		{
			//  << "this will handle full request" << std::endl;
			// std::cout << "method: " << clientPtr->getRequest()->getMethod() << std::endl;
			if (!clientPtr->getRequestError())
				clientPtr->handleCompleteRequest();
			clientPtr->setLastActivityTime(time(NULL));
			clientPtr->setIsRequesting(false);
			struct epoll_event ev;
			ev.events = EPOLLOUT;
			ev.data.fd = fd;
			if (epoll_ctl(epfd, EPOLL_CTL_MOD, fd, &ev) == -1)
				throw_exception("epoll_ctl: ", strerror(errno));
		}
	}
}

void	handleClientResponse(int epfd, int fd, std::map<int, Client*>& clients)
{
	Client* client = clients[fd];
	if (!client->getSentAll())
	{
		client->handleFile();
		Response& currentResponse = client->getResponse();
		std::string res = currentResponse.build();
		// std::cout << "Building res: " << res << std::endl;
		ssize_t sent = send(fd, res.c_str(), res.length(), 0);
		if (sent == -1)
		{
			std::cout << "Sent Error: " << strerror(errno) << std::endl;
			epoll_ctl(epfd, EPOLL_CTL_DEL, fd, NULL);
			delete client;
			clients.erase(fd);
			close(fd);
		}
	}
	else
	{
		std::cout << "Response has been sent" << std::endl;
		epoll_ctl(epfd, EPOLL_CTL_DEL, fd, NULL);
		delete client;
		clients.erase(fd);
		close(fd);
	}
}

void	handleTimeOut(int epfd, std::map<int, Client*>& clients)
{
	std::map<int, Client*>::iterator it;
	for (it = clients.begin(); it != clients.end(); )
	{
		Client* client = it->second;
		if (client && (client->isTimedOut() || (client->getIsCGI() && client->isCgiTimedOut())))
		{
			std::string str;
			if (client->isTimedOut())
				str = "Time out has occured\n";
			else
				str = "CGI Time out has occured\n";
			ssize_t sent = send(it->first, str.c_str(), str.length(), 0);
			if (sent == -1)
			{
				std::cout << "Error: " << strerror(errno) << std::endl;
				epoll_ctl(epfd, EPOLL_CTL_DEL, it->first, NULL);
				close(it->first);
				delete client;
				clients.erase(it++);
			}
			else
			{
				if (client->getIsCGI())
				{
					client->cleanupCGI();
				}
				epoll_ctl(epfd, EPOLL_CTL_DEL, it->first, NULL);
				close(it->first);
				delete client;
				clients.erase(it++);
			}
		}
		else
			it++;
	}
}

void run_server(int epfd, std::map<int, Server*>& servers_fd)
{
    struct epoll_event events[64];
    std::map<int, Client*> clients;

    while (true)
    {
        int nfds = epoll_wait(epfd, events, 64, 1000);
        if (nfds == -1)
            throw_exception("epoll_wait: ", strerror(errno));
		handleTimeOut(epfd, clients);
        for (int i = 0; i < nfds; i++)
        {
            int fd = events[i].data.fd;
            if (listening_fd(servers_fd, fd))
            {
				handleListeningClient(epfd, fd, clients, servers_fd);
				continue;
			}
			if (clients.find(fd) == clients.end())
			{
				epoll_ctl(epfd, EPOLL_CTL_DEL, fd, NULL);
				close(fd);
				continue;
			}
			Client* client = clients[fd];
            if (events[i].events & EPOLLIN)
				handleClientRequest(epfd, fd, clients);
            else if (events[i].events & EPOLLOUT)
            {
                if (client->getIsCGI())
                {
                    client->handleCGI();
                    if (client->getSentAll())
                    {
						client->cleanupCGI();
                        epoll_ctl(epfd, EPOLL_CTL_DEL, fd, NULL);
                        delete client;
                        clients.erase(fd);
                        close(fd);
                    }
                }
                else
                    handleClientResponse(epfd, fd, clients);
            }
        }
    }
}
