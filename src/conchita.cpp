#include <Arduino.h>

#include <LittleFS.h>
#include <HTTPUpdate.h>
#include <map>

#include "conchita.h"
#include "url.h"

void errout(const char *func, const char *txt, int result, Stream &errstr)
{
    errstr.printf("%s: error %d (%s)\n", func, result, txt);
}

void stopClient(Stream &client)
{
    static_cast<WiFiClient &>(client).stop();
}

String Conchita::getCommand()
{
    String result;
    _client.printf("%s> ", _prompt.c_str());

    while (true)
    {
        if (_client.available() > 0)
        {
            int c = (_client.read());
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
        else if (!_client.connected())
        {
            _client.stop();
            result = "";
            break;
        }
        delay(50);
    }
    return result;
}

int Conchita::parse(const char *line, stringArray &argv)
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
        // device.printf("%3d%% (%d/%d)\n", progress, completed, total);
        oldPhase = phase;
    }
}

int wget(stringArray argv, Stream &out)
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
                        out.println("Complete");
                        result = 0;
                    }
                    else
                        out.println("Couldn't create file");
                }
                else
                    out.println("Couldn't create temp file");
            }
            else
                out.printf("Upload failed %d\n", httpCode);
        }
        else
            out.printf("Get failed %s\n", http.errorToString(httpCode).c_str());
    }
    else
        errout(argv[0].c_str(), "upload SERVER PORT SOURCE TARGET", result, out);
    return result;
}

int fsupdate(stringArray argv, Stream &out)
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
            out.printf("FS Update fail error (%d): %s\n",
                       httpUpdate.getLastError(), httpUpdate.getLastErrorString().c_str());
            break;

        case HTTP_UPDATE_NO_UPDATES:
            out.println("No FS update file available");
            break;

        case HTTP_UPDATE_OK:
            Serial.println("FS update available");
            out.println("FS update available");
            result = 0;
            break;
        }
    }
    else
    {
        errout(argv[0].c_str(), "fsupdate URL", result, out);
    }
    return result;
}

int sysupdate(stringArray argv, Stream &out)
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
            out.printf("Update fail error (%d): %s\n",
                       httpUpdate.getLastError(), httpUpdate.getLastErrorString().c_str());
            break;

        case HTTP_UPDATE_NO_UPDATES:
            out.println("No update file available");
            break;

        case HTTP_UPDATE_OK:
            Serial.println("System update available - reseting");
            out.println("System update available - reseting");
            stopClient(out);
            delay(1000);
            ESP.restart();
            result = 0;
            break;
        }
    }
    else
    {
        errout(argv[0].c_str(), "sysupdate URL", result, out);
    }
    return result;
}

void treeRec(File dir, Stream &out)
{
    if (dir)
    {
        dir.size();
        if (dir.isDirectory())
        {
            out.printf("%s :\n", dir.name());
            while (File f = dir.openNextFile())
            {
                treeRec(f, out);
                f.close();
            }
        }
        else
            out.printf(" %6.d %s\n", dir.size(), dir.name());

        dir.close();
    }
}

int rm(stringArray argv, Stream &out)
{
    for (int i = 1; i < argv.size(); i++)
    {
        if (!((LittleFS.remove(argv[i]) || LittleFS.rmdir(argv[i]))))
        {
            out.printf("Could not remove %s\n", argv[i].c_str());
        }
    }
    return 0;
}

int tree(stringArray argv, Stream &out)
{
    File dir = LittleFS.open("/");
    treeRec(dir, out);
    dir.close();
    return 0;
}

int mkdir(stringArray argv, Stream &out)
{
    for (int i = 1; i < argv.size(); i++)
    {
        if (!LittleFS.mkdir(argv[i]))
        {
            out.printf("Could not make %s\n", argv[i].c_str());
        }
    }
    return 0;
}

int cat(stringArray argv, Stream &out)
{
    for (int i = 1; i < argv.size(); i++)
    {
        File f = LittleFS.open(argv[i].c_str());
        if (f)
        {
            if (f.isDirectory())
            {
                out.printf("%s is a directory\n", argv[i].c_str());
                break;
            }
            else
            {
                int c;
                while ((c = f.read()) >= 0)
                    out.write(c);
            }
            f.close();
        }
        else
        {
            out.printf("Could not open %s\n", argv[i].c_str());
        }
    }
    return 0;
}

int reboot(stringArray argv, Stream &out)
{
    out.println("Rebooting");
    out.flush();
    delay(100);
    stopClient(out);
    delay(500);
    ESP.restart();
    return 0;
}

int exit(stringArray argv, Stream &out)
{
    stopClient(out);
    return 0;
}

int Conchita::help(stringArray argv, Stream &out)
{
    if (argv.size() == 1)
    {
        std::map<String, commandDescriptor>::iterator it;

        for (it = commandTable.begin(); it != commandTable.end(); it++)
        {
            out.println(it->first);
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
            out.printf("%s:\n\t%s\n", argv[i].c_str(), cd.helpText.isEmpty() ? "Help not available" : cd.helpText.c_str());
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

Conchita::Conchita(uint16_t port) : _server(port)
{
    buildCommandTable();
}

bool Conchita::start(const uint16_t stacksize, const int priority, const char *name)
{
    _server.begin();

    xTaskCreate(
        staticFunc,
        name,
        stacksize,
        this,
        priority,
        &_handle);
    return true;
}

void Conchita::stop()
{
    if (_handle != NULL) vTaskDelete(_handle);
    _handle = NULL;
}

void Conchita::addCommand(const char *cmd, int (*func)(stringArray, Stream &), const String &helptext)
{
    commandDescriptor desc;
    desc.func = func;
    desc.helpText = helptext;
    commandTable[cmd] = desc;
}

void Conchita::buildCommandTable()
{
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

int Conchita::execute(stringArray argv)
{
    int result = -1;

    if (argv.size() >= 1)
    {
        if (argv[0] == "help")
        {
            help(argv, *this);
        }
        else
            try
            {
                commandDescriptor desc = commandTable.at(argv[0].c_str());
                result = desc.func(argv, *this);
            }
            catch (const std::exception &e)
            {
                errout(argv[0].c_str(), "Command not recognised", result, _client);
            }
    }
    return result;
}

void Conchita::func()
{
    while (true)
    {
        _client = _server.available();
        while (_client)
        {
            String s = getCommand();
            stringArray argv;
            parse(s.c_str(), argv);
            int result = execute(argv);
        }
        delay(500);
    }
}

void Conchita::staticFunc(void *obj)
{
    static_cast<Conchita *>(obj)->func();
}