#pragma once

#include <Arduino.h>

/**
 * @class Url
 * @brief URL parser - Breaks it into its parts
 */
class Url
{
public:
    /**
     * @param url_s The URL as a string
     */
    Url(const char *url_s);
    virtual ~Url();

    const char *getProtocol() { return protocol; }
    const char *getHost() { return host; }
    const uint16_t getPort() { return port; }
    const char *getPath() { return path; }
    const char *getQuery() { return query; }

private:
    void parse(const char *url_s);
    char *protocol;
    char *host;
    char *path;
    char *query;
    uint16_t port;
};
