#include <HTTPUpdate.h>
#include <LITTLEFS.h>

extern WiFiClient client;
extern String archiveServer;
extern String port;

const char* abspath(const char* path, char* result = NULL);

#define BOUNDARY "----BOUNDARY"
#define TIMEOUT 20000

String header(size_t length)
{
    String data;
    data = "POST /upload.php HTTP/1.1\r\n";
    data += "cache-control: no-cache\r\n";
    data += "Content-Type: multipart/form-data; boundary=";
    data += BOUNDARY;
    data += "\r\n";
    data += "Accept: */*\r\n";
    data += "Host: ";
    data += archiveServer;
    data += "\r\n";
    data += "Connection: keep-alive\r\n";
    data += "content-length: ";
    data += String(length);
    data += "\r\n";
    data += "\r\n";
    return (data);
}
String body(String filename)
{
    String data;
    data = "--";
    data += BOUNDARY;
    data += "\r\n";
    data += "Content-Disposition: form-data; name=\"imageFile\"; filename=\"";
    data += filename;
    data += "\"\r\n";
    data += "Content-Type: application/octet-stream\r\n";
    data += "\r\n";

    return (data);
}

bool sendImage(String filename, uint8_t *filedata, size_t filesize)
{
    bool result = true;
    String bodyFile = body(filename);
    String bodyEnd = String("--") + BOUNDARY + String("--\r\n");
    size_t allLen = /* bodyTxt.length()+*/ bodyFile.length() + filesize + bodyEnd.length();
    String headerTxt = header(allLen);
    WiFiClient wclient;
    if (!wclient.connect(archiveServer.c_str(), 80))
    {
        result = false;
    }
    else
    {
        wclient.print(headerTxt + bodyFile);
        wclient.write(filedata, filesize);
        wclient.print("\r\n" + bodyEnd);

        delay(20);
        long tOut = millis() + TIMEOUT;
        while (wclient.connected() && tOut > millis())
        {
            while (wclient.available())
            {
                String d = wclient.readStringUntil('\r');
                client.print(d);
            }
        }
    }
    return result;
}

void push(const int argc, char *argv[])
{
    if ((argc < 2) || (argc > 3))
    {
        client.println("wput source [target]");
    }
    else
    {
        const char* sfilename = abspath(argv[1]);
        File source = LITTLEFS.open(sfilename, "r");
        if (source)
        {
            String content;
            char* target = argv[1];
            if (argc == 3) target = argv[2];
            int c;
            while ((c = source.read()) >= 0)
            {
                content += (char)c;
            }
            if (!sendImage(target, (uint8_t *)content.c_str(), content.length()))
                client.println("Copy failed");

            source.close();
        }
        else
        {
            client.printf("Could not open %s\n", argv[1]);
        }
    }
}

void reportProgress(size_t completed, size_t total)
{
    static int oldPhase = 1;
    int progress = (completed * 100) / total;

    int phase = (progress / 5) % 2; // report at 5% intervals

    if (phase != oldPhase)
    {
        client.printf("%3d%% (%d/%d)\n", progress, completed, total);
        oldPhase = phase;
    }
}

t_httpUpdate_return systemUpdate(const String &url)
{
    WiFiClient httpclient;

    httpUpdate.rebootOnUpdate(false);

    Update.onProgress(reportProgress);

    t_httpUpdate_return ret = httpUpdate.update(httpclient, url);

    switch (ret)
    {
    case HTTP_UPDATE_FAILED:
        client.printf("Update fail error (%d): %s\n",
                      httpUpdate.getLastError(), httpUpdate.getLastErrorString().c_str());
        break;

    case HTTP_UPDATE_NO_UPDATES:
        client.println("No update file available");
        break;

    case HTTP_UPDATE_OK:
        client.println("System update available - reboot to load");
        break;
    }
    return ret;
}

void sysupdate(int argc, char *argv[])
{
    if (argc != 2)
    {
        client.printf("specify source URL\n");
    }

    systemUpdate(argv[1]);
}

void getFile(const char* url, const char* target)
{
    HTTPClient http;
    http.begin(url);
    int httpCode = http.GET();

    if (httpCode > 0)
    {
        if (httpCode == HTTP_CODE_OK)
        {
            File f = LITTLEFS.open(target, "w+");
            if (f)
            {
                String payload = http.getString();
                f.print(payload.c_str());
                f.close();
            }
            else
                client.printf("Could not open %s\n",target);
        }
        else
        {
            client.printf("Upload failed (%d)\n", httpCode);
        }
    }
    else
    {
        client.printf("GET failed, error: %s\n", http.errorToString(httpCode).c_str());
    }
}
