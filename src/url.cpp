#include "url.h"
#include <algorithm>
#include <cctype>
#include <functional>
#include <string.h>

Url::Url(const char* url_s)
{
    protocol = NULL;
    host = NULL;
    path = NULL;
    query = NULL;
    port = 0;
    parse(url_s);
}

Url::~Url()
{
    if (protocol) free(protocol);
    if (host) free(host);
    if (path) free(path);
    if (query) free(query);
}

char* allocAndCopy(const char* source, int l)
{
    char* buff = (char*)malloc(l + 1);
    strncpy(buff, source, l);
    buff[l] = '\00';
    return buff;
}

void Url::parse(const char* url_s)
{
    const char* protDel="://";
    const char* hostDel=":";
    const char* portDel="/";
    const char* pathDel="?";

    const char* protBegin = url_s;
    const char* hostBegin = url_s;
    const char* protEnd = strstr(protBegin,protDel);
    if (protEnd)
    {
        protocol = allocAndCopy(protBegin, protEnd - protBegin);
        hostBegin = protEnd + strlen(protDel);
    }
    const char* hostEnd = strstr(hostBegin, hostDel);
    const char* portBegin = NULL;
    const char* portEnd = NULL;
    if (hostEnd)
    {
        host = allocAndCopy(hostBegin, hostEnd - hostBegin);
        portBegin = hostEnd + strlen(hostDel);
        portEnd = strstr(portBegin, portDel);
        if (portEnd)
        {
            char* portstr = allocAndCopy(portBegin,portEnd - portBegin);
            sscanf(portstr, "%hd", &port);
            free(portstr);
        }
    }
    else
    {
        hostEnd = strstr(hostBegin, portDel);
        if (hostEnd)
        {
            host = allocAndCopy(hostBegin, hostEnd - hostBegin);
        }
    }
    const char* pathBegin = portBegin ? portEnd : hostEnd;
    const char* pathEnd = strstr(pathBegin, pathDel);
    if (pathEnd)
    {
        path = allocAndCopy(pathBegin, pathEnd - pathBegin);
    }
    else
    {
        path = allocAndCopy(pathBegin, strlen(pathBegin));
    }
    if (pathEnd)
    {
        const char* queryBegin = pathEnd + strlen(pathDel);
        query = allocAndCopy(queryBegin, strlen(queryBegin));
    }
}

