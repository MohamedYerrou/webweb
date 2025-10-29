#include <fstream>
#include <sstream>
#include <utility> 
#include <vector>
#include <fstream>
#include <sstream>
#include <utility> 
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
#include "../includes/CGIHandler.hpp"

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
	
	if (received == -1 || received == 0)
	{
		epoll_ctl(epfd, EPOLL_CTL_DEL, fd, NULL);
		delete clientPtr;
		clients.erase(fd);
		close(fd);
	}
	// else if (received == 0)
	// {
	// 	epoll_ctl(epfd, EPOLL_CTL_DEL, fd, NULL);
	// 	delete clientPtr;
	// 	clients.erase(fd);
	// 	close(fd);
	// }
	else
	{
		buf[received] = '\0';
		clientPtr->appendData(buf, received);
		
		if (clientPtr->getReqComplete())
		{
			if (!clientPtr->getRequestError())
				clientPtr->handleCompleteRequest();
			
			if (clientPtr->getCGIHandler() && clientPtr->getCGIHandler()->isStarted() && !clientPtr->getCGIHandler()->isComplete())
			{
				Server* server = clientPtr->getServer();
				
				CGIContext ctx;
				ctx.childpid = clientPtr->getCGIHandler()->getPid();
				ctx.clientfd = fd;
				ctx.body = clientPtr->getRequest()->getBody();
				ctx.bytes_written = 0;
				ctx.output_buffer = "";
				ctx.is_stdin_closed = false;
				ctx.is_stdout_closed = false;
				ctx.pipe_to_cgi = clientPtr->getCGIHandler()->getStdinFd();
				ctx.pipe_from_cgi = clientPtr->getCGIHandler()->getStdoutFd();
				ctx.is_error = false;
				ctx.client = clientPtr;
				
				server->addCgiIn(ctx, epfd);
				server->addCgiOut(ctx, epfd);
				
				struct epoll_event ev;
				ev.events = 0;
				ev.data.fd = fd;
				if (epoll_ctl(epfd, EPOLL_CTL_MOD, fd, &ev) == -1)
					throw_exception("epoll_ctl: ", strerror(errno));
			}
			else
			{
				struct epoll_event ev;
				ev.events = EPOLLOUT;
				ev.data.fd = fd;
				if (epoll_ctl(epfd, EPOLL_CTL_MOD, fd, &ev) == -1)
					throw_exception("epoll_ctl: ", strerror(errno));
			}
		}
	}
}

void    handleClientResponse(int epfd, int fd, std::map<int, Client*>& clients)
{
	Client* client = clients[fd];
	
	if (client->getIsCGI())
	{
		if (client->getCGIHandler() && client->getCGIHandler()->isFinished())
		{
			if (client->getCGIHandler()->is_Error())
			{
				client->errorResponse(500, "CGI script failed");
				
				Response& currentResponse = client->getResponse();
				std::string res = currentResponse.build();
				send(fd, res.c_str(), res.length(), 0);
			}
			else
			{
				std::string rawOutput = client->getCGIHandler()->getBuffer();

				std::string responseBody;
				std::string responseHeaders;

				size_t headerEndPos = rawOutput.find("\r\n\r\n");
				if (headerEndPos == std::string::npos)
					headerEndPos = rawOutput.find("\n\n");

				if (headerEndPos == std::string::npos)
				{
					responseBody = rawOutput;
					std::ostringstream oss;
					oss << responseBody.size();
					responseHeaders = "HTTP/1.1 200 OK\r\nContent-Type: text/html\r\nContent-Length: " + oss.str() + "\r\n";
				}
				else
				{
					responseBody = rawOutput.substr(headerEndPos + (rawOutput[headerEndPos] == '\r' ? 4 : 2));
					std::string cgiHeaderPart = rawOutput.substr(0, headerEndPos);
					
					std::ostringstream ossBodySize;
					ossBodySize << responseBody.size();
					responseHeaders = "HTTP/1.1 ";
					
					size_t statusPos = cgiHeaderPart.find("Status: ");
					if (statusPos != std::string::npos)
					{
						size_t statusEnd = cgiHeaderPart.find("\r\n", statusPos);
						if (statusEnd == std::string::npos) statusEnd = cgiHeaderPart.find('\n', statusPos);
						if (statusEnd == std::string::npos) statusEnd = cgiHeaderPart.length();
						responseHeaders += cgiHeaderPart.substr(statusPos + 8, statusEnd - (statusPos + 8)) + "\r\n";
					}
					else
					{
						responseHeaders += "200 OK\r\n";
					}
					
					responseHeaders += cgiHeaderPart + "\r\n";
					responseHeaders += "Content-Length: " + ossBodySize.str() + "\r\n";
				}
				
				std::string finalResponse = responseHeaders + "\r\n" + responseBody;
				
				send(fd, finalResponse.c_str(), finalResponse.size(), 0);
			}
		}
		else
		{
			return;
		}
	}
	else
	{
		if (!client->getSentAll())
		{
			client->handleFile();
			Response& currentResponse = client->getResponse();
			std::string res = currentResponse.build();
			ssize_t sent = send(fd, res.c_str(), res.length(), 0);
			
			if (sent != -1)
			{
				return;
			}
			// else
			// {
			// 	return;
			// }
		}
	}
	
	epoll_ctl(epfd, EPOLL_CTL_DEL, fd, NULL);
	delete client;
	clients.erase(fd);
	close(fd);
}

void Server::run_server(int epfd, std::map<int, Server*>& servers_fd)
{
	struct epoll_event events[64];
	std::map<int, Client*> clients;

	while (true)
	{
		int nfds = epoll_wait(epfd, events, 64, -1);
		if (nfds == -1)
			throw_exception("epoll_wait: ", strerror(errno));

		for (int i = 0; i < nfds; i++)
		{
			int fd = events[i].data.fd;
			
			if (listening_fd(servers_fd, fd))
			{
				handleListeningClient(epfd, fd, clients, servers_fd);
				continue;
			}

			bool handled = false;

			for (std::map<int, Server*>::iterator sit = servers_fd.begin(); sit != servers_fd.end(); ++sit)
			{
				Server* server = sit->second;
				
				if (server->CGIstdIn.find(fd) != server->CGIstdIn.end())
				{
					handled = true;
					
					Client* cgiClient = server->CGIstdIn[fd].client;
					
					if (events[i].events & (EPOLLERR | EPOLLHUP))
					{
						epoll_ctl(epfd, EPOLL_CTL_DEL, fd, NULL);
						close(fd);
						server->CGIstdIn.erase(fd);
					}
					else if (events[i].events & EPOLLOUT)
					{
						const std::string& body = cgiClient->getRequest()->getBody();
						size_t written_so_far = server->CGIstdIn[fd].bytes_written;
						
						if (written_so_far < body.length())
						{
							ssize_t written = write(fd, body.c_str() + written_so_far, body.length() - written_so_far);
							
							if (written > 0)
							{
								server->CGIstdIn[fd].bytes_written += written;
							}
							else if (written == -1 && errno != EAGAIN && errno != EWOULDBLOCK)
							{
								epoll_ctl(epfd, EPOLL_CTL_DEL, fd, NULL);
								close(fd);
								server->CGIstdIn.erase(fd);
							}
						}
						
						if (server->CGIstdIn.find(fd) != server->CGIstdIn.end() &&
							server->CGIstdIn[fd].bytes_written >= body.length())
						{
							epoll_ctl(epfd, EPOLL_CTL_DEL, fd, NULL);
							close(fd);
							server->CGIstdIn.erase(fd);
						}
					}
					
					break;
				}
				
				if (server->CGIstdOut.find(fd) != server->CGIstdOut.end())
				{
					handled = true;

					std::map<int, CGIContext>::iterator it = server->CGIstdOut.find(fd);
					
					if (clients.find(it->second.clientfd) == clients.end())
					{
						epoll_ctl(epfd, EPOLL_CTL_DEL, fd, NULL);
						close(fd);
						server->CGIstdOut.erase(it);
						break;
					}
					Client* cgiClient = clients[it->second.clientfd];

					if (events[i].events & (EPOLLERR | EPOLLHUP))
					{
						while (true)
						{
							char buf[4096];
							ssize_t bytesRead = read(fd, buf, sizeof(buf));
							if (bytesRead > 0)
							{
								cgiClient->getCGIHandler()->appendResponse(buf, bytesRead);
							}
							else
								break; 
						}
						cgiClient->getCGIHandler()->setComplete(true);
						epoll_ctl(epfd, EPOLL_CTL_DEL, fd, NULL);
						close(fd);
						server->CGIstdOut.erase(it);
						struct epoll_event ev;
						ev.events = EPOLLOUT;
						ev.data.fd = cgiClient->getFD();
						epoll_ctl(epfd, EPOLL_CTL_MOD, cgiClient->getFD(), &ev);
					}
					else if (events[i].events & EPOLLIN)
					{
						while (true)
						{
							char buf[4096];
							ssize_t bytesRead = read(fd, buf, sizeof(buf));
							if (bytesRead > 0)
							{
								cgiClient->getCGIHandler()->appendResponse(buf, bytesRead);
								cgiClient->getCGIHandler()->getBuffer();
							}
							else if (bytesRead == 0)
							{
								cgiClient->getCGIHandler()->setComplete(true);
								epoll_ctl(epfd, EPOLL_CTL_DEL, fd, NULL);
								close(fd);
								server->CGIstdOut.erase(it);
								struct epoll_event ev;
								ev.events = EPOLLOUT;
								ev.data.fd = cgiClient->getFD();
								epoll_ctl(epfd, EPOLL_CTL_MOD, cgiClient->getFD(), &ev);
								break;
							}
							else
							{
								if (errno == EAGAIN || errno == EWOULDBLOCK)
								{
									break;
								}
								cgiClient->getCGIHandler()->setError(true);
								epoll_ctl(epfd, EPOLL_CTL_DEL, fd, NULL);
								close(fd);
								server->CGIstdOut.erase(it);
								struct epoll_event ev;
								ev.events = EPOLLOUT;
								ev.data.fd = cgiClient->getFD();
								epoll_ctl(epfd, EPOLL_CTL_MOD, cgiClient->getFD(), &ev);
								break;
							}
						}
					}

					break;
				}
			}

			if (handled)
				continue;
			
			if (clients.find(fd) == clients.end())
			{
				epoll_ctl(epfd, EPOLL_CTL_DEL, fd, NULL);
				close(fd);
				continue;
			}
			
			if (events[i].events & EPOLLIN)
			{
				char dummy;
				ssize_t peek = recv(fd, &dummy, 1, MSG_PEEK | MSG_DONTWAIT);

				if (peek < 0 && (errno == EAGAIN || errno == EWOULDBLOCK))
				{
				}
				else
				{
					handleClientRequest(epfd, fd, clients);
				}
			}
			else if (events[i].events & EPOLLOUT)
			{
				handleClientResponse(epfd, fd, clients);
			}
			else if (events[i].events & (EPOLLERR | EPOLLHUP))
			{
				handleClientRequest(epfd, fd, clients);
			}
		}
	}
}
