#ifndef CONFIGSTRUCTS_HPP
#define CONFIGSTRUCTS_HPP


#include <string>
#include <vector>
#include <map>

typedef struct Location
{
    std::string							path;
    std::string							root;
    std::string							index;
    std::string							upload_store;
    std::vector<std::string>			methods;
    bool								autoindex;
    std::map<std::string, std::string>	cgi;
	std::pair<int, std::string>			http_redirection;

	Location(): autoindex(false) {}

}				Location;

typedef struct ServerConfig
{
    std::vector<std::pair<std::string, int> >	listens;
    std::map<int, std::string>					errors;
    std::string									root;
    std::string									index;
    bool										autoindex;
    size_t										client_max_body_size;
	std::vector<Location>					locations;

}				ServerConfig;

#endif

class MyException: public std::exception
{
    private:
        std::string msg;
    public:
        MyException(std::string msg): msg(msg){};
        const char* what() const throw() {return msg.c_str();};
        virtual ~MyException() _GLIBCXX_TXN_SAFE_DYN _GLIBCXX_NOTHROW {};
};

