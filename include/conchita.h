#pragma once

#include <Arduino.h>
#include <WiFi.h>
#include <vector>
#include <map>

typedef std::vector<String> stringArray; /** An array of Strings. The command line arguments are passed in this form */

class Conchita
{
public:
    Conchita(uint16_t port);

    bool start(const uint16_t stacksize = 4096, const int priority = 0, const char *name = "Conchita");
    void stop();

    void setPrompt(const char *prompt) { _prompt = prompt; }

    void addCommand(const char *cmd, int (*func)(stringArray, Stream &), const String &helptext = "");

    operator Stream &()
    {
        if (_client.connected())
            return _client;
        else
            return Serial;
    }

private:
    struct commandDescriptor
    {
        int (*func)(stringArray, Stream &);
        String helpText;
    };

    int help(stringArray argv, Stream &out);
    void buildCommandTable();
    int parse(const char *line, stringArray &argv);
    String getCommand();
    int execute(stringArray argv);

    WiFiServer _server;
    WiFiClient _client;

    void func();
    static void staticFunc(void *);
    TaskHandle_t _handle;
    String _prompt;
    std::map<String, commandDescriptor> commandTable;
};