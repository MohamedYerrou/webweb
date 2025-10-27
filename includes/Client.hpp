#ifndef CLIENT_HPP
#define CLIENT_HPP

#include <string>
#include <iostream>
#include <unistd.h>
#include "Request.hpp"
#include "Response.hpp"
#include "CGIHandler.hpp"
#include "Server.hpp"
#include "Location.hpp"
#include <sys/stat.h>
#include <sys/socket.h>
#include <arpa/inet.h>
#include <netinet/in.h>
#include <sys/wait.h>
#include "Utils.hpp"
#include <algorithm>
#include <fstream>
#include <dirent.h>
#include <cstdio>

class Client
{
	private:
		int				fd;
		std::ifstream	fileStream;
		std::string 	headers;
		std::string		body;
		size_t			bodySize;
		bool			endHeaders;
		bool			reqComplete;
		bool			hasBody;
		bool			requestError;
		Request*		currentRequest;
		Server*			currentServer;
		const Location* location;
		Response		currentResponse;
		bool			sentAll;
		bool			fileOpened;
		bool			oneBody;
		std::string		boundary;
		std::string		endBoundry;
		bool			inBody;
		bool			finishBody;

		// --- CGI State ---
		CGIHandler* cgiHandler;
		bool		isCGI;
		std::string newPath;
		int			cgi_out_fd; // Added: FD for CGI's stdout
		int			cgi_in_fd;  // Added: FD for CGI's stdin
		size_t		cgi_body_bytes_written; // Added: Track POST body write
		bool		cgi_body_sent; // Added: True when POST body is fully sent
		bool		cgi_headers_parsed; // Added: True when CGI headers are parsed
		std::string	cgi_response_buffer; // Added: Buffers data from cgi_out_fd

	public:
		Client(int fd, Server* srv);
		~Client();
		int					getFD() const;
		const std::string&	getHeaders() const;
		bool				getEndHeaders() const;
		bool				getReqComplete() const;
		void				appendData(const char* buf, ssize_t length);
		void				setBodySize(size_t size);
		void				handleHeaders(const std::string& raw);
		void				handleBody(const char* buf, ssize_t length);
		Request*			getRequest() const;
		void				handleCompleteRequest();
		Response& 			getResponse();
		bool				getRequestError() const;
		bool				getSentAll() const;
		void				setSentAll(bool flag);
		bool    			getContentType();
		
		//handle methods
		const Location*	findMathLocation(std::string url);
		const Location* findBestMatch(const std::string uri);
		std::string		joinPath();
		void			handleGET();
        void            handleDELETE();
        void            handleDeleteFile(std::string totalPath);
        void            handleDeleteDir(std::string totalPath);
		bool			allowedMethod(const std::string& method);
		void			handleRedirection();
		void			errorResponse(int code, const std::string& error);
		void			handleDirectory(const std::string& path);
		void			handleFile();
		void			listingDirectory(std::string path);
		std::string		constructFilePath(std::string uri);
		void    		PrepareResponse(const std::string& path);

		
		// --- Modified CGI ---
		std::vector<std::string> buildCGIEnv(const std::string& scriptPath);
		void	checkCGIValid();
		void 	setupCGI(); // Added: Replaces old handleCGI
		bool	getIsCGI() const { return isCGI; }
		void 	setIsCGI(bool val) {isCGI = val; }
		
		CGIHandler* getCGIHandler(); // Added
		int 	getCGIOutFD() const; // Added
		int 	getCGIInFD() const;  // Added
		void 	setCGIOutFD(int fd); // Added
		void 	setCGIInFD(int fd);  // Added

		bool 	isCGIBodySent() const; // Added
		void 	setCGIBodySent(bool val); // Added
		size_t* getCGIBytesWrittenPtr(); // Added
		
		bool 	areCGIHeadersParsed() const; // Added
		void 	parseCGIHeaders(); // Added
		std::string& getCGIResponseBuffer(); // Added

		void 	cleanupCGI();
};

#endif