#include <Arduino.h>
#include <WiFi.h>
#include <LittleFS.h>
#include <HTTPUpdate.h>
#include <vector>

#include "conchita.h"
// #include "config.h"
#include "url.h"

String error;
WiFiServer cliServer(1685);
WiFiClient cliClient;

const uint16_t cliStackSize = 4096; //* @todo needs to be configurable by the caller

struct commandDescriptor
{
    int (*func)(stringArray);
    String helpText;
};

std::map<String, commandDescriptor> commandTable;

String getCommand()
{
    String result;
    cliClient.printf("%s> ", "psu");

    while (true)
    {
        if (cliClient.available() > 0)
        {
            int c = (cliClient.read());
            if (c == '\n')
                break;
            else if (c == '\r')
                ;
            else
            {
                if (isprint(c))
                    result += (char)c;
                else
                {
                }
            }
        }
        else if (!cliClient.connected())
        {
            cliClient.stop();
            result = "";
            break;
        }
        delay(50);
    }
    return result;
}

int parse(const char *line, stringArray &argv)
{
    String arg;
    bool quoting = false;
    bool quotechar = false;

    int len = strlen(line);
    for (int i = 0; i < len; i++)
    {
        char c = line[i];

        if (c == '"' && !quotechar)
        {
            quoting = !quoting;
            continue;
        }
        else if (!quoting && !quotechar)
        {
            if (c == '\\')
            {
                quotechar = true;
                continue;
            }
            else if (isspace(c))
            {
                if (arg.length() > 0)
                    argv.push_back(arg);
                arg = "";
                continue;
            }
        }
        quotechar = false;
        arg += c;
    }
    if (arg.length() > 0)
        argv.push_back(arg);

    return 0;
}

void reportProgress(size_t completed, size_t total)
{
    const int interval = 10;
    static int oldPhase = 1;
    int progress = (completed * 100) / total;

    int phase = (progress / interval) % 2; // report at 5% intervals

    if (phase != oldPhase)
    {
        Serial.printf("%3d%% (%d/%d)\n", progress, completed, total);
        cliClient.printf("%3d%% (%d/%d)\n", progress, completed, total);
        oldPhase = phase;
    }
}

int wget(stringArray argv)
{
    int result = -1;
    if ((argv.size() >= 2) && (argv.size() < 4))
    {
        String url = argv[1];
        String target;
        if (!url.startsWith("http://"))
            url = "http://" + url;
        if (argv.size() == 3)
        {
            target = argv[2];
        }
        else
        {
            int index = url.lastIndexOf("/");
            target = url.substring(index);
        }
        if (!target.startsWith("/"))
            target = "/" + target;

        HTTPClient http;

        // http.begin(url); using this form adds 150K odd to the size of the image!
        // So instead:

        Url u(url.c_str());
        uint16_t port = u.getPort();
        if (port == 0)
            port = 80;
        http.begin(u.getHost(), port, u.getPath());

        int httpCode = http.GET();

        if (httpCode > 0)
        {
            if (httpCode == HTTP_CODE_OK)
            {
                File f = LittleFS.open("/upload.tmp", "w+");
                if (f)
                {
                    Stream &s = http.getStream();
                    uint8_t buffer[128];
                    size_t total = http.getSize();
                    size_t sofar = 0;
                    size_t l;
                    while ((l = s.readBytes(buffer, sizeof(buffer) - 1)))
                    {
                        f.write(buffer, l);
                        reportProgress(sofar, total);
                        sofar += l;
                    }
                    f.close();
                    if (LittleFS.rename("/upload.tmp", target))
                    {
                        Serial.println("Complete");
                        cliClient.println("Complete");
                        result = 0;
                    }
                    else
                        cliClient.println("Couldn't create file");
                }
                else
                    cliClient.println("Couldn't create temp file");
            }
            else
                cliClient.printf("Upload failed %d\n", httpCode);
        }
        else
            cliClient.printf("Get failed %s\n", http.errorToString(httpCode).c_str());
    }
    else
        error = "upload SERVER PORT SOURCE TARGET";
    return result;
}

int fsupdate(stringArray argv)
{
    int result = -1;
    if (argv.size() == 2)
    {
        String url = argv[1];
        if (!url.startsWith("http://"))
            url = "http://" + url;

        WiFiClient httpclient;

        Update.onProgress(reportProgress);
        Serial.println("FS update started");

        t_httpUpdate_return ret = httpUpdate.updateSpiffs(httpclient, url);

        switch (ret)
        {
        case HTTP_UPDATE_FAILED:
            cliClient.printf("FS Update fail error (%d): %s\n",
                             httpUpdate.getLastError(), httpUpdate.getLastErrorString().c_str());
            break;

        case HTTP_UPDATE_NO_UPDATES:
            cliClient.println("No FS update file available");
            break;

        case HTTP_UPDATE_OK:
            Serial.println("FS update available");
            cliClient.println("FS update available");
            result = 0;
            break;
        }
    }
    else
    {
        error = "fsupdate URL";
    }
    return result;
}

int sysupdate(stringArray argv)
{
    int result = -1;
    if (argv.size() == 2)
    {
        String url = argv[1];
        if (!url.startsWith("http://"))
            url = "http://" + url;

        WiFiClient httpclient;

        httpUpdate.rebootOnUpdate(false);

        Update.onProgress(reportProgress);
        Serial.println("System update started");

        t_httpUpdate_return ret = httpUpdate.update(httpclient, url);

        switch (ret)
        {
        case HTTP_UPDATE_FAILED:
            cliClient.printf("Update fail error (%d): %s\n",
                             httpUpdate.getLastError(), httpUpdate.getLastErrorString().c_str());
            break;

        case HTTP_UPDATE_NO_UPDATES:
            cliClient.println("No update file available");
            break;

        case HTTP_UPDATE_OK:
            Serial.println("System update available - reseting");
            cliClient.println("System update available - reseting");
            cliClient.stop();
            delay(1000);
            ESP.restart();
            result = 0;
            break;
        }
    }
    else
    {
        error = "sysupdate URL";
    }
    return result;
}

void treeRec(File dir)
{
    if (dir)
    {
        dir.size();
        if (dir.isDirectory())
        {
            cliClient.printf("%s :\n", dir.name());
            while (File f = dir.openNextFile())
            {
                treeRec(f);
                f.close();
            }
        }
        else
            cliClient.printf(" %6.d %s\n", dir.size(), dir.name());

        dir.close();
    }
}

int rm(stringArray argv)
{
    for (int i = 1; i < argv.size(); i++)
    {
        if (!((LittleFS.remove(argv[i]) || LittleFS.rmdir(argv[i]))))
        {
            cliClient.printf("Could not remove %s\n", argv[i].c_str());
        }
    }
    return 0;
}

int tree(stringArray argv)
{
    File dir = LittleFS.open("/");
    treeRec(dir);
    dir.close();
    return 0;
}

int mkdir(stringArray argv)
{
    for (int i = 1; i < argv.size(); i++)
    {
        if (!LittleFS.mkdir(argv[i]))
        {
            cliClient.printf("Could not make %s\n", argv[i].c_str());
        }
    }
    return 0;
}

int cat(stringArray argv)
{
    for (int i = 1; i < argv.size(); i++)
    {
        File f = LittleFS.open(argv[i].c_str());
        if (f)
        {
            if (f.isDirectory())
            {
                cliClient.printf("%s is a directory\n", argv[i].c_str());
                break;
            }
            else
            {
                int c;
                while ((c = f.read()) >= 0)
                    cliClient.write(c);
            }
            f.close();
        }
        else
        {
            cliClient.printf("Could not open %s\n", argv[i].c_str());
        }
    }
    return 0;
}

int reboot(stringArray argv)
{
    cliClient.println("Rebooting");
    cliClient.flush();
    delay(100);
    cliClient.stop();
    delay(500);
    ESP.restart();
    return 0;
}

int exit(stringArray argv)
{
    cliClient.stop();
    return 0;
}

int help(stringArray argv)
{
    if (argv.size() == 1)
    {
        std::map<String, commandDescriptor>::iterator it;

        for (it = commandTable.begin(); it != commandTable.end(); it++)
        {
            cliClient.println(it->first);
        }
    }
    else
    {
        for (int i = 1; i < argv.size(); i++)
        {
            commandDescriptor cd;
            try
            {
                cd = commandTable.at(argv[i]);
            }
            catch (const std::exception &e)
            {
                cd.helpText = "Not recognised";
            }
            cliClient.printf("%s:\n\t%s\n", argv[i].c_str(), cd.helpText.isEmpty() ? "Help not available" : cd.helpText.c_str());
        }
    }
    return 0;
}

void msToTime(unsigned long v, char *t)
{
    int h = v / (1000 * 60 * 60);
    int m = (v / (1000 * 60)) % 60;
    int s = (v / 1000) % 60;
    int ms = v % 1000;

    sprintf(t, "%02d:%02d:%02d.%03d", h, m, s, ms);
}

void addCommand(const char *cmd, int (*func)(stringArray), const String helptext)
{
    commandDescriptor desc;
    desc.func = func;
    desc.helpText = helptext;

    commandTable[cmd] = desc;
}

/**
 * @todo Make this configurable
 */
void buildCommandTable()
{
    addCommand("help", help);
    addCommand("exit", exit);
    addCommand("reboot", reboot);

    addCommand("sysupdate", sysupdate);
    addCommand("fsupdate", fsupdate);
    addCommand("wget", fsupdate);

    addCommand("tree", tree);
    addCommand("rm", rm);
    addCommand("mkdir", mkdir);
    addCommand("cat", cat);
}

int execute(stringArray argv)
{
    int result = -1;

    if (argv.size() >= 1)
    {
        try
        {
            commandDescriptor desc = commandTable.at(argv[0].c_str());
            result = desc.func(argv);
        }
        catch (const std::exception &e)
        {
            error = "Command not recognised";
        }
    }
    return result;
}

void cliFunc(void *)
{
    cliServer.begin();

    buildCommandTable();

    while (true)
    {
        cliClient = cliServer.available();
        while (cliClient)
        {
            String s = getCommand();
            stringArray argv;
            parse(s.c_str(), argv);
            int result = execute(argv);
            if (result < 0)
            {
                cliClient.printf("%s: error %d (%s)\n", argv[0].c_str(), result, error.c_str());
            }
        }
        delay(500);
    }
}

TaskHandle_t startAsTask(const char *name, const UBaseType_t priority)
{
    TaskHandle_t taskHandle;

    xTaskCreate(
        cliFunc,
        name,
        cliStackSize,
        NULL,
        priority,
        &taskHandle);

    return taskHandle;
}
