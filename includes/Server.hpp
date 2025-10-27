#ifndef SERVER_HPP
#define SERVER_HPP

#include <string>
#include <vector>
#include <map>
#include "Location.hpp"


class Server
{
	private:
	std::vector<std::pair<std::string, int> >	listens;
	std::vector<Location>						locations;
	size_t										client_max_body_size;
	std::vector<int>							listen_fd;
		
	public:
	Server();
	~Server();
	void							push_listen(std::pair<std::string, int>);
	void							push_location(Location);
	void							init_server(int epfd, std::map<int, Server*>& fd_vect);
	const std::vector<Location>&	getLocations() const;
	size_t							getMaxSize() const;
};

#endif