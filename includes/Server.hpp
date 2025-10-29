#ifndef SERVER_HPP
#define SERVER_HPP

#include <string>
#include <vector>
#include <map>
#include "Location.hpp"

class Client;
typedef struct s_CGIContext 
{
	int childpid;
	int clientfd;
	std::string body;
	size_t bytes_written;
	std::string output_buffer;
	bool is_stdin_closed;
	bool is_stdout_closed;
	int pipe_to_cgi;
	int pipe_from_cgi;
	bool is_error;
	Client* client;
} CGIContext;
class Server
{
	private:
	std::vector<std::pair<std::string, int> >	listens;
	std::vector<Location>						locations;
	size_t										client_max_body_size;
	std::vector<int>							listen_fd;
	std::map<int , CGIContext> CGIstdIn;
	std::map<int , CGIContext> CGIstdOut;

	public:
	Server();
	~Server();
	void static						run_server(int epfd, std::map<int, Server*>& servers_fd);
	void							push_listen(std::pair<std::string, int>);
	void							push_location(Location);
	void							init_server(int epfd, std::map<int, Server*>& fd_vect);
	const std::vector<Location>&	getLocations() const;
	size_t							getMaxSize() const;
	void	addCgiIn(CGIContext CGIctx, int fd);
	void	addCgiOut(CGIContext CGIctx, int fd);
};

#endif