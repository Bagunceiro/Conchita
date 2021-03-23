#include <Arduino.h>
#include <vector>
#include <WiFi.h>
#include <ESPmDNS.h>
#include <lfs.h>
#include <LITTLEFS.h>
#include <esp_wps.h>

// const char *ssid = "asgard_2g";
// const char *password = "enaLkraP";
const char *mDNSName = "conchita";

WiFiServer wifiServer(1685);
WiFiClient client;

bool redirect_append = false;

Stream *StdOut = &client;
Stream *StdIn = &client;
Stream *StdErr = &client;

/*
Stream *StdOut = &Serial;
Stream *StdIn  = &Serial;
Stream *StdErr = &Serial;
*/

using namespace std;

namespace pa
{
    extern char *optarg;
    extern int optind;
    extern int getopt(int argc, char *const argv[], const char *optstring);
    extern void resetgetopt();
}

String outFile;
String inFile;

String archiveServer("192.168.0.101");
uint16_t archivePort = 80;

String CWD('/');
const char *prompt = "> ";

const int MAX_PATH_LENGTH = LFS_NAME_MAX; // LFS_NAME_MAX is 255

/*
 * get a character from StdIn
 */
const int getChar()
{
    int c = -2;
    do
    {
        if ((!client) && (StdIn == &client))
        {
            Serial.println("force stop");
            c = -1;
        }
        if (StdIn->available() > 0)
        {
            c = (StdIn->read());
            if (c == '\004')
            {
                c = -1;
            }
        }
        else
            delay(10);
    } while (c == -2);
    return c;
}

/*
 * Read a command line from the telnet client
 */
String getCommand()
{
    String result;

    while (true)
    {
        if (client.available() > 0)
        {
            int c = (client.read());
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
        else if (!client.connected())
        {
            client.stop();
            result = "";
            break;
        }
        delay(50);
    }
    return result;
}

/*
Chop a dynamically allocated buffer containing a null terminated string down to
the size required. The string does not have to be at the start of the buffer - pos
indicates where it starts.
*/
char *shrinkBuffer(char *buffer, int pos = 0)
{
    char *newbuff = (char *)malloc(strlen(&buffer[pos]) + 1);
    strcpy(newbuff, &buffer[pos]);
    free(buffer);
    return newbuff;
}

/*
    NOTE: abspath, basename and dirname use a static buffer. To make them
    reentrant provide an adequately sized (MAX_PATH_LENGTH + 1) buffer as the
    second parameter
*/
/*
 * Convert relative pathname to full absolute path (as required by the littlefs
 * library functions)
 */
const char *abspath(const char *path, char *result = NULL)
{
    static char *buffer = NULL;

    if (buffer != NULL)
    {
        free(buffer);
        buffer = NULL;
    }

    if (result == NULL)
    {
        buffer = (char *)malloc(MAX_PATH_LENGTH + 1);
        result = buffer;
    }
    vector<String> v;
    String base;

    if (result == NULL)
        result = buffer;
    if (path[0] == '/')
        strncpy(result, path, MAX_PATH_LENGTH);
    else
        snprintf(result, MAX_PATH_LENGTH, "%s%s%s", CWD.c_str(), CWD.endsWith("/") ? "" : "/", path);

    for (int i = 0; i <= strlen(result); i++)
    {
        if ((result[i] == '/') || (result[i] == '\00'))
        {
            if (base.length() > 0)
            {
                if (base == ".")
                {
                }
                else if (base == "..")
                {
                    v.pop_back();
                }
                else
                {
                    v.push_back(base);
                }
                base = "";
            }
        }
        else
            base += result[i];
    }

    if (v.size() == 0)
    {
        strcpy(result, "/");
    }
    else
        result[0] = '\00';
    for (String s : v)
    {
        strcat(result, "/");
        strcat(result, s.c_str());
    }
    if (buffer)
    {
        buffer = shrinkBuffer(buffer);
        result = buffer;
    }
    return result;
}

/*
 * removes the directory portion of a qualified pathname
 */
const char *basename(const char *path, char *result = NULL)
{
    static char *buffer = NULL;

    if (buffer != NULL)
    {
        free(buffer);
        buffer = NULL;
    }

    if (result == NULL)
    {
        buffer = (char *)malloc(MAX_PATH_LENGTH + 1);
        result = buffer;
    }
    strncpy(result, path, MAX_PATH_LENGTH);
    int pos = strlen(result) + 1;
    while (--pos >= 0)
    {
        if (result[pos - 1] == '/')
            break;
    }
    if (pos < 0)
    {
        pos = 0;
    }

    char *name = &result[pos];

    if (buffer)
    {
        buffer = shrinkBuffer(buffer, pos);
        name = buffer;
    }
    return name;
}

/*
 * Returns the directory portion of a qualified file name.
 */
const char *dirname(const char *path, char *result = NULL)
{
    static char *buffer = NULL;

    if (buffer != NULL)
    {
        free(buffer);
        buffer = NULL;
    }

    if (result == NULL)
    {
        buffer = (char *)malloc(MAX_PATH_LENGTH + 1);
        result = buffer;
    }
    if (result == NULL)
        result = buffer;
    strncpy(result, path, MAX_PATH_LENGTH);
    int pos = strlen(result);
    while (--pos >= 0)
    {
        char c = result[pos];
        result[pos] = '\0';
        if (c == '/')
        {
            if (pos == 0)
                result[pos] = '/';
            break;
        }
    }
    if (buffer)
    {
        buffer = shrinkBuffer(buffer);
        result = buffer;
    }
    return result;
}

void wpsInit()
{
    esp_wps_config_t wpsconfig;

    wpsconfig.crypto_funcs = &g_wifi_default_wps_crypto_funcs;
    wpsconfig.wps_type = WPS_TYPE_PBC;
    strcpy(wpsconfig.factory_info.manufacturer, "PA");
    strcpy(wpsconfig.factory_info.model_number, "1");
    strcpy(wpsconfig.factory_info.model_name, "Conchita");
    strcpy(wpsconfig.factory_info.device_name, "ESP32");
    esp_wifi_wps_enable(&wpsconfig);
    esp_wifi_wps_start(0);
}

void WiFiEvent(WiFiEvent_t event, system_event_info_t info)
{
    switch (event)
    {
    case SYSTEM_EVENT_STA_START:
        break;
    case SYSTEM_EVENT_STA_GOT_IP:
        Serial.println("Connected to : " + String(WiFi.SSID()));
        Serial.print("Got IP: ");
        Serial.println(WiFi.localIP());
        break;
    case SYSTEM_EVENT_STA_DISCONNECTED:
        // Serial.printf("%lu Disconnected from station\n", millis());
        WiFi.reconnect();
        break;
    case SYSTEM_EVENT_STA_WPS_ER_SUCCESS:
        Serial.println("WPS Successful : " + WiFi.SSID() + "/" + WiFi.psk());
        esp_wifi_wps_disable();
        delay(10);
        WiFi.begin();
        break;
    case SYSTEM_EVENT_STA_WPS_ER_FAILED:
        Serial.println("WPS Failed");
        esp_wifi_wps_disable();
        break;
    case SYSTEM_EVENT_STA_WPS_ER_TIMEOUT:
        Serial.println("WPS Timed out");
        esp_wifi_wps_disable();
        break;
    default:
        break;
    }
}

void setup()
{
    Serial.begin(9600);
    delay(500);

    WiFi.mode(WIFI_STA);
    WiFi.onEvent(WiFiEvent);

    unsigned long then = millis();
    enum
    {
        MODE_NONE,
        MODE_CONN,
        MODE_WPS
    } mode = MODE_NONE;

    /*
     * Until we have a connection alternate between attempt to
     *  - connect to last used AP (for 5 seconds) and
     *  - WPS (for 30 seconds).
     */
    while (WiFi.status() != WL_CONNECTED)
    {
        unsigned long sinceThen = millis() - then;
        if (sinceThen < 5000) // first 5 secs try simply connecting to AP
        {
            if (mode != MODE_CONN)
            {
                mode = MODE_CONN;
                Serial.printf("%lu Connect to WiFi\n", millis());
                WiFi.begin(); // In case of mergency can take (ssid, password);
            }
        }
        else if (sinceThen < 35000) // then try 30 seconds of WPS
        {
            if (mode != MODE_WPS)
            {
                mode = MODE_WPS;
                Serial.printf("%lu Try WPS\n", millis());
                WiFi.disconnect();
                wpsInit();
            }
        }
        else // reset all parameters for another cycle
        {
            mode = MODE_NONE;
            then = millis();
            WiFi.disconnect();
            esp_wifi_wps_disable();
        }
        yield();
    }

    // esp_wifi_wps_disable();

    Serial.println("Connected to WiFi");
    Serial.println(WiFi.localIP());
    MDNS.begin(mDNSName);
    MDNS.addService("littlesh", "tcp", 24);
    wifiServer.begin();
    LITTLEFS.begin(true);
}

void lsline(File f)
{
    String n = f.name();
    String c = CWD;
    if (!c.endsWith("/"))
    {
        c += '/';
    }

    char type;
    if (n.startsWith(c))
    {
        n = n.substring(c.length());
    }

    f.seek(0, SeekMode::SeekEnd);
    size_t size = f.position();
    char szbuff[8];

    if (f.isDirectory())
    {
        szbuff[0] = '\0';
        type = 'd';
    }
    else
    {
        sprintf(szbuff, "%u", size);
        type = 'f';
    }

    StdOut->printf("%c %6s %s\n", type, szbuff, n.c_str());
}

void lsfile(const char *basename)
{
    const char *fn = abspath(basename);
    File dir = LITTLEFS.open(fn);
    if (dir)
    {
        if (dir.isDirectory())
        {
            while (File f = dir.openNextFile())
            {
                lsline(f);
                f.close();
            }
        }
        dir.close();
    }
    else
        StdErr->printf("Could not open %s\n", fn);
}

void ls(const int argc, char *argv[])
{

    if (argc == 1)
    {
        lsfile(CWD.c_str());
    }
    else
    {
        for (int i = 1; i < argc; i++)
        {
            lsfile(argv[i]);
        }
    }
}

int getBuffer(char buffer[], int &bpos, const int bufsize)
{
    int c;
    bpos = 0;
    while ((c = getChar()) >= 0)
    {
        buffer[bpos++] = c;
        if ((bpos >= bufsize) || (c == '\n') || (c == '\r'))
        {
            return 0;
        }
    }
    return -1;
}

void cat(int argc, char *argv[])
{
    if (argc == 1)
    {
        int bufsize = 42;
        int bpos;
        char buffer[bufsize];
        while (getBuffer(buffer, bpos, bufsize) >= 0)
        {
            StdOut->write(buffer, bpos);
        }
        return;
    }
    for (int i = pa::optind; i < argc; i++)
    {
        const char *fn = abspath(argv[i]);

        while (File f = LITTLEFS.open(fn))
        {
            if (f)
            {
                if (f.isDirectory())
                {
                    StdErr->printf("%s is a directory\n", fn);
                    break;
                }
                else
                {
                    int c;
                    while ((c = f.read()) >= 0)
                        StdOut->write(c);
                }
                f.close();
            }
            else
                StdErr->printf("Could not open %s\n", fn);
            break;
        }
    }
}

bool rmfile(const char *fn, bool recursive)
{
    // StdErr->printf("rmfile(%s,%d)\n", fn, recursive);
    const char *fntmp = abspath(fn);
    char fullname[strlen(fn) + 1];
    strcpy(fullname, fntmp);
    bool result = true;

    if (!LITTLEFS.remove(fullname))
    {
        // It's probably a directory
        if (recursive)
        {
            File f;
            while (f = LITTLEFS.open(fullname))
            {
                if (f.isDirectory())
                {
                    File child = f.openNextFile();
                    if (child)
                    {
                        const char *cname = child.name();
                        child.close();
                        result = rmfile(cname, recursive);
                    }
                }
                f.close();
                LITTLEFS.rmdir(fullname);
            }
        }
        else
        {
            result = false;
            StdErr->printf("Could not remove %s\n", fullname);
        }
    }
    return result;
}

void rm(int argc, char *argv[])
{
    bool recursive = false;

    int opt;
    while ((opt = pa::getopt(argc, argv, "r")) != -1)
    {
        switch (opt)
        {
        case 'r':
            recursive = true;
            break;
        case '?':
        default:
            StdErr->println("Unknown option");
            break;
        }
    }

    for (int i = pa::optind; i < argc; i++)
    {
        rmfile(argv[i], recursive);
    }
}

void touch(int argc, char *argv[])
{
    for (int i = 1; i < argc; i++)
    {
        const char *fn = abspath(argv[i]);
        File f = LITTLEFS.open(fn, "a+");
        if (f)
        {
            f.close();
        }
        else
        {
            StdErr->printf("Could not open %s\n", fn);
        }
    }
}

void mkdir(int argc, char *argv[])
{
    for (int i = 1; i < argc; i++)
    {
        const char *fn = abspath(argv[i]);
        if (!LITTLEFS.mkdir(fn))
        {
            StdErr->printf("Could not make %s\n", fn);
        }
    }
}

void rmdir(int argc, char *argv[])
{
    for (int i = pa::optind; i < argc; i++)
    {
        const char *fn = abspath(argv[i]);
        if (!LITTLEFS.rmdir(fn))
        {
            StdErr->printf("Could not remove %s\n", fn);
        }
    }
}

void cd(int argc, char *argv[])
{
    if (argc > 2)
    {
        StdErr->println("Too many arguments");
    }
    else
    {
        const char *fn = "/";
        if (argc == 2)
            fn = argv[1];
        fn = abspath(fn);
        File dir = LITTLEFS.open(fn);
        if (dir)
        {
            if (dir.isDirectory())
            {
                CWD = fn;
            }
            else
                StdErr->println("Not a directory");
            dir.close();
        }
        else
        {
            StdErr->println("No such directory");
        }
    }
}

void pwd(int argc, char *argv[])
{
    if (argc != 1)
    {
        StdErr->println("Too many arguments");
    }
    else
    {
        StdOut->println(CWD);
    }
}

void reboot(int argc, char *argv[])
{
    if (argc != 1)
    {
        StdErr->println("Too many arguments");
    }
    else
    {
        StdErr->println("System going down now");
    }
    ESP.restart();
}

void getFile(const char *url, const char *target);

void wget(int argc, char *argv[])
{
    if (argc != 3)
    {
        StdErr->printf("wget url target\n");
    }
    else
    {
        char *fname = argv[2];
        getFile(argv[1], abspath(fname));
    }
}

void pull(int argc, char *argv[])
{
    if ((argc < 2) || (argc > 3))
    {
        StdErr->printf("pull source [target]\n");
    }
    else
    {
        StdErr->println("Not yet implemented");
    }
}

bool copyfile(const char *source, const char *target)
{
    StdErr->printf("copyfile(\"%s\",\"%s\");\n", source, target);
    File sfile = LITTLEFS.open(source, "r");
    bool result = false;
    if (sfile)
    {
        File tfile = LITTLEFS.open(target, "w+");
        if (tfile)
        {
            int c;
            while ((c = sfile.read()) >= 0)
            {
                tfile.write(c);
            }
            result = true;
            tfile.close();
        }
        sfile.close();
    }
    else
        StdErr->printf("Could not open %s for read\n", source);
    return result;
}

void cp(int argc, char *argv[])
{
    bool recursive = false;
    int opt;

    while ((opt = pa::getopt(argc, argv, "r")) != -1)
    {
        switch (opt)
        {
        case 'r':
            recursive = true;
            break;
        case '?':
        default:
            StdErr->println("Unknown option");
            break;
        }
    }

    if (recursive)
    {
        StdErr->println("Do it recursively");
    }

    if ((argc - pa::optind) < 2)
    {
        StdErr->printf("cp FILE... TARGET\n");
    }

    bool targetIsDir = false;

    const char *tmptarget = abspath(argv[argc - 1]);
    char *target = (char *)malloc(strlen(tmptarget) + 1);
    strcpy(target, tmptarget);

    if (LITTLEFS.exists(target))
    {
        File tf = LITTLEFS.open(target);
        if (tf)
        {
            targetIsDir = tf.isDirectory();
            tf.close();
        }
    }
    else
    {
        if (argc > 3)
        {
            if (!targetIsDir)
            {
                StdErr->printf("%s not a directory\n", argv[argc - 1]);
                goto exit;
            }
        }
    }

    if (targetIsDir)
    {
        for (int i = pa::optind; i < (argc - 1); i++)
        {
            const char *tgtfile = basename(argv[i]);
            char tgt[strlen(target) + strlen(tgtfile) + 2];
            strcpy(tgt, target);
            strcat(tgt, "/");
            strcat(tgt, tgtfile);
            const char *src = abspath(argv[i]);
            copyfile(src, tgt);
        }
    }
    else
    {
        copyfile(abspath(argv[pa::optind]), target);
    }
exit:
    free(target);
}

void mv(int argc, char *argv[])
{
    bool targetIsDir = false;

    const char *tmptarget = abspath(argv[argc - 1]);
    char *target = (char *)malloc(strlen(tmptarget) + 1);
    strcpy(target, tmptarget);

    if (LITTLEFS.exists(target))
    {
        File tf = LITTLEFS.open(target);
        if (tf)
        {
            targetIsDir = tf.isDirectory();
            tf.close();
        }
    }
    if (targetIsDir)
    {
        for (int i = pa::optind; i < (argc - 1); i++)
        {
            const char *tgtfile = basename(argv[i]);
            char tgt[strlen(target) + strlen(tgtfile) + 2];
            strcpy(tgt, target);
            strcat(tgt, "/");
            strcat(tgt, tgtfile);
            const char *src = abspath(argv[i]);

            if (!LITTLEFS.rename(src, tgt))
                StdErr->printf("Could not move %s to %s\n", src, target);
        }
    }
    else
    {
        const char *src = abspath(argv[pa::optind]);
        if (!LITTLEFS.rename(src, target))
            StdErr->printf("Could not rename %s to %s\n", src, target);
    }
}

void archive(int argc, char *argv[])
{
    if (argc > 3)
    {
        StdErr->printf("archive [server]\n");
    }
    if (argc == 3)
    {
        archiveServer = argv[2];
    }
    else
    {
        StdErr->printf("Archive server: %s\n", archiveServer.c_str());
    }
}

void exit(int argc, char *argv[])
{
    client.stop();
}

void bn(int argc, char *argv[])
{
    for (int i = 1; i < argc; i++)
    {
        StdOut->println(abspath(argv[i]));
        StdOut->println(basename(argv[i]));
        StdOut->println(dirname(argv[i]));
    }
}

void sysupdate(int argc, char *argv[]);
void push(int argc, char *argv[]);
void help(int argc, char *argv[]);

#define cmdEntry(C, D, S) \
    {                     \
#C, C, #D, #S     \
    }

struct
{
    const char *cmd;
    void (*func)(int, char *[]);
    const char *description;
    const char *synopsis;
} cmdTable[] = {
    cmdEntry(archive, set or read the archive server name, archive[SERVER]),
    cmdEntry(cat, concatenate files and print on the standard output, cat[FILE]...),
    cmdEntry(cd, change working directory, ),
    cmdEntry(cp, copy file, ),
    cmdEntry(exit, , ),
    cmdEntry(help, , ),
    cmdEntry(ls, , ),
    cmdEntry(mkdir, , ),
    cmdEntry(mv, , ),
    cmdEntry(push, , ),
    cmdEntry(reboot, , ),
    cmdEntry(rm, , ),
    cmdEntry(rmdir, , ),
    cmdEntry(pull, , ),
    cmdEntry(pwd, , ),
    cmdEntry(sysupdate, , ),
    cmdEntry(touch, , ),
    cmdEntry(wget, , ),
    cmdEntry(bn, , ),
    {NULL, NULL, NULL, NULL}};

void help(int argc, char *argv[])
{
    for (int i = 0; cmdTable[i].cmd; i++)
    {
        StdOut->print(cmdTable[i].cmd);
        StdOut->print('\t');
        StdOut->println(cmdTable[i].description);
    }
}

bool execute(std::vector<String> &args)
{
    bool result = true;
    File _stdout;
    File _stdin;
    pa::resetgetopt();

    if (inFile != "")
    {
        const char *p = abspath(inFile.c_str());
        _stdin = LITTLEFS.open(p, "r");
        if (_stdin)
        {
            StdIn = &_stdin;
        }
        else
        {
            client.printf("Cannot open %s for redirect\n", outFile.c_str());
        }
    }
    if (outFile != "")
    {
        const char *p = abspath(outFile.c_str());
        _stdout = LITTLEFS.open(p, redirect_append ? "a+" : "w+");
        if (_stdout)
        {
            StdOut = &_stdout;
        }
        else
        {
            client.printf("Cannot open %s for redirect\n", outFile.c_str());
        }
    }

    if (args.size() >= 1)
    {
        const char *cmd = args[0].c_str();
        int argc = args.size();
        char *argv[argc];
        for (int i = 0; i < argc; i++)
        {
            argv[i] = (char *)malloc(args[i].length() + 1);
            strcpy(argv[i], args[i].c_str());
        }
        bool foundCmd = false;
        for (int i = 0; cmdTable[i].cmd; i++)
        {
            // client.printf("compare -%s-%s-\n", cmd, cmdTable[i].cmd);
            if (strcmp(cmd, cmdTable[i].cmd) == 0)
            {
                foundCmd = true;
                cmdTable[i].func(argc, argv);
                break;
            }
        }
        if (!foundCmd)
        {
            client.printf("Command '%s' not found\n", cmd);
            result = false;
        }

        for (int i = 0; i < argc; i++)
        {
            free(argv[i]);
        }
    }
    if (_stdout)
        _stdout.close();
    if (_stdin)
        _stdin.close();

    StdIn = &client;
    StdOut = &client;

    return result;
}

enum redirect_t
{
    REDIRECT_NONE = 0,
    REDIRECT_OUT,
    REDIRECT_APPEND,
    REDIRECT_IN
};

void addArg(String &arg, vector<String> &argv, redirect_t redirect)
{
    if (arg.length() > 0)
    {
        switch (redirect)
        {
        case REDIRECT_OUT:
            outFile = arg;
            redirect_append = false;
            break;
        case REDIRECT_APPEND:
            outFile = arg;
            redirect_append = true;
            break;
        case REDIRECT_IN:
            inFile = arg;
            break;
        default:
            argv.push_back(arg);
        }
    }
}

int parse(const char *line, vector<String> &argv)
{
    String arg;
    bool quoting = false;
    bool quotechar = false;
    redirect_t redirect = REDIRECT_NONE;
    outFile = "";
    inFile = "";

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
            else if (c == '>')
            {
                if (redirect == REDIRECT_OUT)
                    redirect = REDIRECT_APPEND;
                else
                    redirect = REDIRECT_OUT;
                continue;
            }
            if (c == '<')
            {
                redirect = REDIRECT_IN;
                continue;
            }
            else if (isspace(c))
            {
                addArg(arg, argv, redirect);
                arg = "";
                continue;
            }
        }
        quotechar = false;
        arg += c;
    }
    addArg(arg, argv, redirect);

    return 0;
}

void loop()
{
    client = wifiServer.available();
    bool conn = false;
    while (client)
    {
        if (!conn)
        {
            Serial.println("Client connected");
            conn = true;
        }
        //        if (client.connected())
        //        {
        std::vector<String> args;

        client.print(CWD.c_str());
        client.print(prompt);
        String l = getCommand();
        parse(l.c_str(), args);
        execute(args);
        //        }
    }
    if (conn)
        Serial.println("Client disconnected");
}