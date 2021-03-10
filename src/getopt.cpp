#include <Arduino.h>

namespace pa
{

    char *optarg = (char *)0;
    int optind = 1;
    int posinarg = 0;

    void resetgetopt()
    {
        optarg = (char *)0;
        optind = 1;
        posinarg = 0;
    }

    int getopt(int argc, char *const argv[], const char *optstring)
    {
        static int posinarg = 0;
        int result = -1;

    doitagain:
        if (optind < argc)
        {
            char *arg = argv[optind];

            if (strcmp(arg, "-") == 0 || strcmp(arg, "--") == 0)
            {
                optind++;
                posinarg = 0;
                return -1;
            }

            if ((posinarg) || (arg[0] == '-'))
            {
                posinarg++;
                char opt = arg[posinarg];
                if (opt)
                {
                    result = '?'; // Unless we find it later
                    for (int j = 0; char c = optstring[j]; j++)
                    {
                        if (c != ':') // just ignore colons
                        {
                            if (opt == c)
                            {
                                result = opt; // matched
                                
                                bool wantsparam = (optstring[j + 1] == ':');
                                if (wantsparam)
                                {
                                    optarg = &arg[posinarg + 1];
                                    if (optarg[0] == '\00')
                                    {
                                        optind++;
                                        if (optind < argc)
                                        {
                                            optarg = argv[optind];
                                            optind++;
                                            posinarg = 0;
                                        }
                                    }
                                    else
                                    {
                                        optind++;
                                        posinarg = 0;
                                    }
                                }
                                return result;
                            }
                        }
                    }
                    return result;
                }
                else // Move on to the next arg
                {
                    optind++;
                    posinarg = 0;
                    goto doitagain;
                }
            }
        }
        return result;
    }

}