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
#include <map> // Added
#include "../includes/Server.hpp"
#include "../includes/Client.hpp"
#include "../includes/Utils.hpp"

// --- Forward Declarations for new handlers ---
void handleCGIRead(int epfd, Client* client, std::map<int, Client*>& clients);
void handleCGIWrite(int epfd, Client* client, std::map<int, Client*>& clients);
void cleanupClient(int epfd, int fd, std::map<int, Client*>& clients);


void	handleListeningClient(int epfd, int fd, std::map<int, Client*>& clients, std::map<int, Server*>& servers_fd)
{
	int client_fd = accept(fd, NULL, NULL);
	if (client_fd == -1)
	{
		std::cerr << "accept: " << strerror(errno) << std::endl;
		return;
	}
	setNonBlocking(client_fd);

	struct epoll_event ev;
	ev.events = EPOLLIN;
	ev.data.fd = client_fd;
	if (epoll_ctl(epfd, EPOLL_CTL_ADD, client_fd, &ev) == -1)
	{
		close(client_fd);
		std::cerr << "epoll_ctl (client): " << strerror(errno) << std::endl;
		return;
	}

	// Map the client_fd to a new Client object
	clients[client_fd] = new Client(client_fd, servers_fd.find(fd)->second);
}

// --- New Cleanup Function ---
void cleanupClient(int epfd, int fd, std::map<int, Client*>& clients)
{
    std::map<int, Client*>::iterator it = clients.find(fd);
    if (it == clients.end())
        return; // Already cleaned up

    Client* client = it->second;
    std::cout << "Cleaning up client (socket: " << client->getFD() << ")" << std::endl;

    // Remove all FDs associated with this client from epoll and the map
    
    // 1. Client Socket
    epoll_ctl(epfd, EPOLL_CTL_DEL, client->getFD(), NULL);
    close(client->getFD());
    if (clients.count(client->getFD()))
        clients.erase(client->getFD());

    // 2. CGI Out Pipe
    if (client->getCGIOutFD() != -1)
    {
        epoll_ctl(epfd, EPOLL_CTL_DEL, client->getCGIOutFD(), NULL);
        if (clients.count(client->getCGIOutFD()))
            clients.erase(client->getCGIOutFD());
    }
    // 3. CGI In Pipe
    if (client->getCGIInFD() != -1)
    {
        epoll_ctl(epfd, EPOLL_CTL_DEL, client->getCGIInFD(), NULL);
        if (clients.count(client->getCGIInFD()))
            clients.erase(client->getCGIInFD());
    }

    // 4. Delete the client object (which deletes cgiHandler)
    delete client; 
}


void	handleClientRequest(int epfd, int fd, std::map<int, Client*>& clients)
{
	Client* clientPtr = clients[fd]; // We know fd is the client socket
	char	buf[65536]; // Larger buffer

	ssize_t received = recv(clientPtr->getFD(), buf, sizeof(buf) - 1, 0);
	if (received <= 0)
	{
        std::cout << "Client disconnected or recv error (fd: " << fd << ")" << std::endl;
		cleanupClient(epfd, fd, clients);
		return;
	}

	buf[received] = '\0';
	clientPtr->appendData(buf, received);

	if (clientPtr->getReqComplete())
	{
		if (!clientPtr->getRequestError())
			clientPtr->handleCompleteRequest(); // This calls setupCGI if needed
		
        struct epoll_event ev;

		if (clientPtr->getIsCGI() && clientPtr->getCGIHandler())
		{
			// --- This is a CGI request ---
			
			// 1. Add CGI_OUT pipe to epoll (for reading CGI response)
			if (clientPtr->getCGIOutFD() != -1)
			{
				ev.events = EPOLLIN;
				ev.data.fd = clientPtr->getCGIOutFD();
				if (epoll_ctl(epfd, EPOLL_CTL_ADD, ev.data.fd, &ev) == -1)
				{
					std::cerr << "epoll_ctl (cgi_out): " << strerror(errno) << "\n";
					clientPtr->errorResponse(500, "Failed to poll CGI");
					clientPtr->setIsCGI(false); // Fallback to static error
				}
				else
				{
					clients[ev.data.fd] = clientPtr; // Map cgi_out_fd to client
				}
			}

			// 2. Add CGI_IN pipe to epoll if there's a body to write
			if (clientPtr->getCGIInFD() != -1)
			{
				ev.events = EPOLLOUT;
				ev.data.fd = clientPtr->getCGIInFD();
				if (epoll_ctl(epfd, EPOLL_CTL_ADD, ev.data.fd, &ev) == -1)
                {
                    std::cerr << "epoll_ctl (cgi_in): " << strerror(errno) << "\n";
                    clientPtr->errorResponse(500, "Failed to poll CGI");
					clientPtr->setIsCGI(false); // Fallback to static error
                }
                else
                {
				    clients[ev.data.fd] = clientPtr; // Map cgi_in_fd to client
                }
			}
		}

		// 3. Modify Client socket to write (for *both* CGI and static)
		ev.events = EPOLLOUT;
		ev.data.fd = clientPtr->getFD();
		if (epoll_ctl(epfd, EPOLL_CTL_MOD, ev.data.fd, &ev) == -1)
		{
			std::cerr << "epoll_ctl (client mod): " << strerror(errno) << "\n";
			cleanupClient(epfd, fd, clients);
		}
	}
}

void	handleClientResponse(int epfd, int fd, std::map<int, Client*>& clients)
{
	Client* client = clients[fd];
	Response& response = client->getResponse();

	if (client->getIsCGI())
    {
        // --- Handle sending CGI response to client ---
        CGIHandler* cgi = client->getCGIHandler();
        if (!cgi) { cleanupClient(epfd, fd, clients); return; }

        // We can only send data if CGI headers are parsed
        if (!client->areCGIHeadersParsed())
        {
            if (cgi->isFinished())
            {
                // CGI finished but we *still* don't have headers?
                client->parseCGIHeaders(); // Try one last parse
                if (!client->areCGIHeadersParsed())
                {
                    // CGI script was bad (e.g., syntax error, no headers).
                    client->errorResponse(502, "Bad Gateway: Malformed CGI response");
                    client->setIsCGI(false); // Fallback to static error sending
                    // We don't recurse, EPOLLOUT will fire again
                }
            }
            return; // Headers not ready, just wait. EPOLLOUT will fire again.
        }

        // 1. Send headers if not already sent.
        if (!response.getSentHeaders())
        {
            std::string headers = response.buildHeaders();
            ssize_t sent = send(fd, headers.c_str(), headers.length(), 0);
            if (sent == -1) { cleanupClient(epfd, fd, clients); return; }
            response.setSentHeaders(true);
        }

        // 2. Send body data from CGI buffer
        std::string& cgi_body_buf = client->getCGIResponseBuffer();
        if (!cgi_body_buf.empty())
        {
            ssize_t sent = send(fd, cgi_body_buf.c_str(), cgi_body_buf.length(), 0);
            if (sent == -1) { cleanupClient(epfd, fd, clients); return; }
            
            cgi_body_buf.erase(0, sent); // Erase what was sent
        }

        // 3. If CGI is finished and buffer is empty, close connection
        if (cgi->isFinished() && cgi_body_buf.empty())
        {
            std::cout << "CGI response fully sent." << std::endl;
            cleanupClient(epfd, fd, clients);
        }
    }
	else
	{
		// --- Handle sending static file response (FIXED) ---
		
        // 1. Send headers + first chunk if not sent
        if (!response.getSentHeaders())
        {
            client->handleFile(); // Load first chunk / prepare headers
            std::string res = response.build(); // Builds headers + first body chunk
            ssize_t sent = send(fd, res.c_str(), res.length(), 0);
            if (sent == -1) { cleanupClient(epfd, fd, clients); return; }
            // build() sets sentHeaders=true
        }
		// 2. Send subsequent chunks
		else if (!client->getSentAll())
		{
			client->handleFile(); // Load next chunk
			std::string bodyChunk = response.build(); // build() now just returns body
			if (!bodyChunk.empty())
			{
				ssize_t sent = send(fd, bodyChunk.c_str(), bodyChunk.length(), 0);
				if (sent == -1) { cleanupClient(epfd, fd, clients); return; }
			}
		}

		// 3. If all sent, clean up
		if (client->getSentAll())
		{
			std::cout << "Static response has been sent" << std::endl;
			cleanupClient(epfd, fd, clients);
		}
	}
}

// --- NEW CGI HANDLER: Read from CGI script's stdout ---
void handleCGIRead(int epfd, Client* client, std::map<int, Client*>& clients)
{
    CGIHandler* cgi = client->getCGIHandler();
    if (!cgi) { cleanupClient(epfd, client->getFD(), clients); return; }
    
    cgi->readOutput(); // This reads all available data into cgi->buffer
    
    // Move data from cgiHandler's buffer to client's buffer
    client->getCGIResponseBuffer().append(cgi->getBuffer());
    cgi->clearBuffer();
    
    // If headers not parsed, try to parse them now
    if (!client->areCGIHeadersParsed())
    {
        client->parseCGIHeaders();
    }

    if (cgi->isFinished())
    {
        std::cout << "CGI script finished, pipe EOF on fd: " << cgi->getOutFD() << std::endl;
        // EOF from CGI. Process is done.
        epoll_ctl(epfd, EPOLL_CTL_DEL, cgi->getOutFD(), NULL);
        clients.erase(cgi->getOutFD());
        client->setCGIOutFD(-1); // Mark as closed
    }
}

// --- NEW CGI HANDLER: Write to CGI script's stdin ---
void handleCGIWrite(int epfd, Client* client, std::map<int, Client*>& clients)
{
    if (client->isCGIBodySent())
        return;

    CGIHandler* cgi = client->getCGIHandler();
    Request* req = client->getRequest();
    if (!cgi || !req) { cleanupClient(epfd, client->getFD(), clients); return; }
    
    const std::string& body = req->getBody();
    size_t total_size = body.length();
    size_t* bytes_written = client->getCGIBytesWrittenPtr();
    
    if (*bytes_written >= total_size)
    {
        // Should be caught by isCGIBodySent, but for safety
        cgi->closeInFD();
        epoll_ctl(epfd, EPOLL_CTL_DEL, cgi->getInFD(), NULL);
        clients.erase(cgi->getInFD());
        client->setCGIInFD(-1);
        client->setCGIBodySent(true);
        return;
    }

    size_t bytes_to_write = total_size - *bytes_written;
    ssize_t n = write(cgi->getInFD(), body.c_str() + *bytes_written, bytes_to_write);

    if (n > 0)
    {
        *bytes_written += n;
    }
    else if (n == -1)
    {
        if (errno != EAGAIN && errno != EWOULDBLOCK)
        {
            // Real write error (e.g., pipe broken)
            std::cerr << "[CGI] write() error: " << strerror(errno) << "\n";
            cgi->closeInFD();
            epoll_ctl(epfd, EPOLL_CTL_DEL, cgi->getInFD(), NULL);
            clients.erase(cgi->getInFD());
            client->setCGIInFD(-1);
            client->setCGIBodySent(true); // Stop trying
        }
        // If EAGAIN, just return. EPOLLOUT will fire again.
    }

    if (*bytes_written >= total_size)
    {
        // Done writing body
        cgi->closeInFD();
        epoll_ctl(epfd, EPOLL_CTL_DEL, cgi->getInFD(), NULL);
        clients.erase(cgi->getInFD());
        client->setCGIInFD(-1);
        client->setCGIBodySent(true);
    }
}


// --- THE MAIN SERVER LOOP (MODIFIED) ---
void run_server(int epfd, std::map<int, Server*>& servers_fd)
{
    struct epoll_event events[64];
    std::map<int, Client*> clients; // Maps *all* FDs to a Client

    while (true)
    {
        int nfds = epoll_wait(epfd, events, 64, -1);
        if (nfds == -1)
        {
            if (errno == EINTR) // Interrupted by signal, just continue
                continue;
            throw_exception("epoll_wait: ", strerror(errno));
        }

        for (int i = 0; i < nfds; i++)
        {
            int fd = events[i].data.fd;
            
            if (listening_fd(servers_fd, fd))
            {
                handleListeningClient(epfd, fd, clients, servers_fd);
                continue;
            }

            std::map<int, Client*>::iterator it = clients.find(fd);
            if (it == clients.end())
            {
                // Stale FD, kernel probably cleaned it up
                epoll_ctl(epfd, EPOLL_CTL_DEL, fd, NULL);
                continue;
            }
            Client* client = it->second;

            // --- LOGIC REORDERED ---

            // Handle I/O first
            // EPOLLIN will catch normal reads *and* pipe closures (HUP)
            if (events[i].events & EPOLLIN)
            {
                if (fd == client->getFD())
                    handleClientRequest(epfd, fd, clients);
                else if (fd == client->getCGIOutFD())
                    handleCGIRead(epfd, client, clients);
            }
            
            // Handle writability
            // Note: Not 'else if', as a socket can be readable and writable
            else if (events[i].events & EPOLLOUT)
            {
                // Check if client still exists, as EPOLLIN might have cleaned it up
                if (clients.find(fd) == clients.end())
                    continue;

                if (fd == client->getFD())
                    handleClientResponse(epfd, fd, clients);
                else if (fd == client->getCGIInFD())
                    handleCGIWrite(epfd, client, clients);
            }
            
            // Handle errors *last*
            // Only triggers if not IN or OUT, or if IN/OUT handlers failed
            // and didn't clean up.
            else if (events[i].events & (EPOLLERR | EPOLLHUP))
            {
                // Check if client still exists
                if (clients.find(fd) == clients.end())
                    continue;

                // We must differentiate: EPOLLHUP on a *read* pipe (cgi_out)
                // is handled by handleCGIRead (as EPOLLIN|EPOLLHUP).
                // A "pure" HUP or an ERR is a true error.
                if (!(events[i].events & EPOLLIN))
                {
                    std::cout << "EPOLLERR or pure EPOLLHUP on fd: " << fd << std::endl;
                    cleanupClient(epfd, fd, clients); // Pass the FD that failed
                }
            }
            // --- END REORDER ---
        }
    }
}