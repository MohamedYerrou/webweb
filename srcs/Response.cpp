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

void	Response::setEndHeaders(bool flag)
{
    sentHeaders = flag;
}

std::string    Response::build()
{
    std::string res;
    if (!sentHeaders)
    {
        res += _protocol + " " + intTostring(codeStatus) + " " + textStatus + "\r\n";
        std::map<std::string, std::string>::iterator it;
        for (it = headers.begin(); it != headers.end(); it++)
            res += it->first + ": " + it->second + "\r\n";
        res += "\r\n";
        res += body;
        sentHeaders = true;
    }
    else
        res += body;
    return res;
}
