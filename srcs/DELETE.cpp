#include "../includes/Client.hpp"

void    Client::handleDeleteDir(std::string totalPath)
{
    if (isEmpty(totalPath))
    {
        if (std::remove(totalPath.c_str()) == 0)
        {
            currentResponse = Response();
            currentResponse.setProtocol(currentRequest->getProtocol());
            currentResponse.setStatus(204, getStatusText(204));
            currentResponse.setHeaders("Date", currentDate());
            currentResponse.setHeaders("Connection", "close");
        }
        else
            errorResponse(500, "Internal Server Error");
    }
    else
        errorResponse(409, "Can not DELETE non empty directory");
}

void    Client::handleDeleteFile(std::string totalPath)
{
    if (allowedDelete(totalPath))
    {
        if (std::remove(totalPath.c_str()) == 0)
        {
            currentResponse = Response();
            currentResponse.setProtocol(currentRequest->getProtocol());
            currentResponse.setStatus(204, getStatusText(204));
            currentResponse.setHeaders("Date", currentDate());
            currentResponse.setHeaders("Connection", "close");
        }
        else
            errorResponse(500, "Internal Server Error");
    }
    else
        errorResponse(403, "DELETE OPERATION FORBIDEN");
}

void    Client::handleDELETE()
{
    if (location)
    {
        if (!allowedMethod("DELETE"))
        {
            errorResponse(405, "Method not allowed");
            return;
        }
        if (location->getRoot().empty())
        {
            errorResponse(500, "Missing root directive");
            return;
        }
        std::string totalPath = joinPath();
        std::cout << "total path: " << totalPath << std::endl;
        if (isDir(totalPath))
            handleDeleteDir(totalPath);
        else if (isFile(totalPath))
            handleDeleteFile(totalPath);
        else
            errorResponse(404, "NOT FOUND");
    }
}