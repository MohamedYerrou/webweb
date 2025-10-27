#include "../includes/Response.hpp"
#include "../includes/Utils.hpp"

Response::Response()
    : sentHeaders(false)
{
}

Response::~Response()
{
}

void    Response::setProtocol(std::string protocol)
{
    _protocol = protocol;
}

void    Response::setStatus(int code, std::string text)
{
    codeStatus = code;
    textStatus = text;
}

void    Response::setHeaders(std::string key, std::string value)
{
    headers[key] = value;
}

void    Response::setBody(std::string body)
{
    this->body = body;
}

// Added
std::string Response::getBody() const
{
	return body;
}

// Renamed from setEndHeaders
void	Response::setSentHeaders(bool flag)
{
    sentHeaders = flag;
}

// Added
bool Response::getSentHeaders() const
{
	return sentHeaders;
}

// Added
std::string    Response::buildHeaders()
{
    std::string res;
    res += _protocol + " " + intTostring(codeStatus) + " " + textStatus + "\r\n";
    std::map<std::string, std::string>::iterator it;
    for (it = headers.begin(); it != headers.end(); it++)
        res += it->first + ": " + it->second + "\r\n";
    res += "\r\n";
    return res;
}


std::string    Response::build()
{
    std::string res;
    if (!sentHeaders)
    {
        res = buildHeaders(); // Use new function
        res += body;
        sentHeaders = true;
    }
    else
        res += body; // Only append body if headers already sent
    
    // Clear body after building, as it's now "sent"
    // This is important for chunked sending.
    body.clear(); 
    return res;
}