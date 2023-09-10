#pragma once

#include <Arduino.h>
#include <vector>
#include <map>

typedef std::vector<String> stringArray; /* An array of Strings. The command line arguments are passed in this form */

    void addCommand(const char *cmd, int (*func)(stringArray), const String helptext = "");
    void cliFunc(void *);
    TaskHandle_t starCLIAsTask(const char *name, const UBaseType_t priority);