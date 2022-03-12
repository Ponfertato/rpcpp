#include <iostream>
#include <csignal>
#include <thread>
#include <unistd.h>
#include <time.h>
#include <regex>
#include <fstream>
#include <filesystem>

// Discord RPC
#include "discord/discord.h"

// X11 libs
#include <X11/Xlib.h>
#include <X11/Xatom.h>
#include <X11/Xutil.h>

// variables
#define VERSION "2.0"

namespace
{
    volatile bool interrupted{false};
}
namespace fs = std::filesystem;
using namespace std;
int startTime;
Display *disp;
float mem, cpu = 0;
string distro;
static int trapped_error_code = 0;
string wm;

vector<string> apps = {"blender", "code-oss", "com_usebottles_bottles", "discord", "firefox", "gimp", "inkscape", "krita", "lutris", "spotify-client", "steam", "superproductivity",
                        "system-file-manager", "telegram", "utilities-terminal", "cuttlefish", "electron", "jetbrains-toolbox", "com_jetbrains_clion", "com_jetbrains_datagrip",
                        "com_jetbrains_goland", "com_jetbrains_intellij-idea-community", "com_jetbrains_intellij-idea-ultimate", "com_jetbrains_phpstorm", "com_jetbrains_pycharm-community",
                        "com_jetbrains_pycharm-professional", "com_jetbrains_rider", "com_jetbrains_rubymine", "com_jetbrains_webstorm", "onlyoffice-desktopeditors", "plasmadiscover",
                        "systemsettings", "system-run", "com_mojang_minecraft", "com_sublimemerge_app", "com_sublimetext_three", "kate", "ark"};   // currently supported app icons on discord rpc (replace if you made your own discord application)
map<string, string> aliases = {{"vscodium", "code-oss"}, {"code", "code-oss"}, {"code-[a-z]+", "code-oss"}, {"bottles", "com_usebottles_bottles"}, {"spotify", "spotify-client"},
                        {"dolphin", "system-file-manager"}, {"telegramdesktop", "telegram"}, {"konsole", "utilities-terminal"}, {"electron+[a-z]+", "electron"}, {"jetbrains-clion", "com_jetbrains_clion"},
                        {"jetbrains-datagrip", "com_jetbrains_datagrip"}, {"jetbrains-goland", "com_jetbrains_goland"}, {"jetbrains-intellij-idea-community", "com_jetbrains_intellij-idea-community"},
                        {"jetbrains-intellij-idea-ultimate", "com_jetbrains_intellij-idea-ultimate"}, {"jetbrains-phpstorm", "com_jetbrains_phpstorm"}, {"jetbrains-pycharm-community", "com_jetbrains_pycharm-community"},
                        {"jetbrains-pycharm-professional", "com_jetbrains_pycharm-professional"}, {"jetbrains-rider", "com_jetbrains_rider"}, {"jetbrains-rubymine", "com_jetbrains_rubymine"},
                        {"jetbrains-webstorm", "com_jetbrains_webstorm"}, {"desktopeditors","onlyoffice-desktopeditors"}, {"discover", "plasmadiscover"}, {"plasmashell", "system-run"}};  // for apps with different names, plasma apps included
map<string, string> distros = {{"Arch", "archlinux"}}; // only arch system on rpc
string helpMsg = string(
                     "Usage:\n") +
                 " rpcpp [options]\n\n" +
                 "Options:\n" +
                 " -f, --ignore-discord   don't check for discord on start\n" +
                 " --debug                print debug messages\n" +
                 " --usage-sleep=5000     sleep time in milliseconds between updating cpu and ram usages\n" +
                 " --update-sleep=100     sleep time in milliseconds between updating the rich presence and focused application\n\n" +
                 " -h, --help             display this help and exit\n" +
                 " -v, --version          output version number and exit";

struct DiscordState
{
    discord::User currentUser;

    unique_ptr<discord::Core> core;
};

struct DistroAsset
{
    string image;
    string text;
};

struct WindowAsset
{
    string image;
    string text;
};

struct StartOptions
{
    bool ignoreDiscord = false;
    bool debug = false;
    int usageSleep = 5000;
    int updateSleep = 300;
    bool printHelp = false;
    bool printVersion = false;
};

StartOptions options;

// methods

static int error_handler(Display *display, XErrorEvent *error)
{
    trapped_error_code = error->error_code;
    return 0;
}

string lower(string s)
{
    transform(s.begin(), s.end(), s.begin(),
              [](unsigned char c)
              { return tolower(c); });
    return s;
}

double ms_uptime(void)
{
    FILE *in = fopen("/proc/uptime", "r");
    double retval = 0;
    char tmp[256] = {0x0};
    if (in != NULL)
    {
        fgets(tmp, sizeof(tmp), in);
        retval = atof(tmp);
        fclose(in);
    }
    return retval;
}

float getRAM()
{
    ifstream meminfo;
    meminfo.open("/proc/meminfo");

    long total = 0;
    long available = 0;

    regex memavailr("MemAvailable: +(\\d+) kB");
    regex memtotalr("MemTotal: +(\\d+) kB");
    smatch matcher;

    string line;

    while (getline(meminfo, line))
    {
        if (regex_search(line, matcher, memavailr))
        {
            available = stoi(matcher[1]);
        }
        else if (regex_search(line, matcher, memtotalr))
        {
            total = stoi(matcher[1]);
        }
    }

    meminfo.close();

    if (total == 0)
    {
        return 0;
    }
    return (float)(total - available) / total * 100;
}

void setActivity(DiscordState &state, string details, string sstate, string smallimage, string smallimagetext, string largeimage, string largeimagetext, long uptime, discord::ActivityType type)
{
    time_t now = time(nullptr);
    discord::Activity activity{};
    activity.SetDetails(details.c_str());
    activity.SetState(sstate.c_str());
    activity.GetAssets().SetSmallImage(smallimage.c_str());
    activity.GetAssets().SetSmallText(smallimagetext.c_str());
    activity.GetAssets().SetLargeImage(largeimage.c_str());
    activity.GetAssets().SetLargeText(largeimagetext.c_str());
    activity.GetTimestamps().SetStart(uptime);
    activity.SetType(type);

    state.core->ActivityManager().UpdateActivity(activity, [](discord::Result result)
                                                 { if(options.debug) cout << ((result == discord::Result::Ok) ? "Succeeded" : "Failed")
                                                        << " updating activity!\n"; });
}

string getActiveWindowClassName(Display *disp)
{
    Atom classreq = XInternAtom(disp, "WM_CLASS", False), type;
    int form;
    unsigned long remain, len;
    unsigned char *list;

    Atom request = XInternAtom(disp, "_NET_ACTIVE_WINDOW", False);
    Window root = XDefaultRootWindow(disp);
    Atom actualtype;
    int actualformat;
    unsigned long nitems;
    unsigned long bytes_after; /* unused */
    unsigned char *prop;
    int status = XGetWindowProperty(disp, root, request, 0, (~0L), False, AnyPropertyType, &actualtype, &actualformat, &nitems, &bytes_after,
                                    &prop);

    if (nitems == 0)
    {
        XFree(prop);

        return "";
    }

    XClassHint hint;
    XGetClassHint(disp, *((Window *)prop), &hint);
    XFree(hint.res_name);
    XFree(prop);
    string s(hint.res_class);
    XFree(hint.res_class);

    return s;
}

static unsigned long long lastTotalUser, lastTotalUserLow, lastTotalSys, lastTotalIdle;

void getLast()
{
    FILE *file = fopen("/proc/stat", "r");
    fscanf(file, "cpu %llu %llu %llu %llu", &lastTotalUser, &lastTotalUserLow,
           &lastTotalSys, &lastTotalIdle);
    fclose(file);
}

double getCPU()
{
    getLast();
    sleep(1);
    double percent;
    FILE *file;
    unsigned long long totalUser, totalUserLow, totalSys, totalIdle, total;

    file = fopen("/proc/stat", "r");
    fscanf(file, "cpu %llu %llu %llu %llu", &totalUser, &totalUserLow,
           &totalSys, &totalIdle);
    fclose(file);

    if (totalUser < lastTotalUser || totalUserLow < lastTotalUserLow ||
        totalSys < lastTotalSys || totalIdle < lastTotalIdle)
    {
        // Overflow detection. Just skip this value.
        percent = -1.0;
    }
    else
    {
        total = (totalUser - lastTotalUser) + (totalUserLow - lastTotalUserLow) +
                (totalSys - lastTotalSys);
        percent = total;
        total += (totalIdle - lastTotalIdle);
        percent /= total;
        percent *= 100;
    }

    lastTotalUser = totalUser;
    lastTotalUserLow = totalUserLow;
    lastTotalSys = totalSys;
    lastTotalIdle = totalIdle;

    return percent;
}

bool processRunning(string name, bool ignoreCase = true)
{

    string strReg = "\\/" + name + " ?";
    regex nameRegex;
    smatch progmatcher;

    if (ignoreCase)
        nameRegex = regex(strReg, regex::icase);

    else
        nameRegex = regex(strReg);

    string procs;

    regex processRegex("\\/proc\\/\\d+\\/cmdline");
    smatch isProcessMatcher;

    std::string path = "/proc";
    for (const auto &entry : fs::directory_iterator(path))
    {
        if (fs::is_directory(entry.path()))
        {
            for (const auto &entry2 : fs::directory_iterator(entry.path()))
            {
                string path = entry2.path();
                if (regex_search(path, isProcessMatcher, processRegex))
                {
                    ifstream s;
                    s.open(entry2.path());
                    string line;
                    while (getline(s, line))
                    {
                        if (regex_search(line, progmatcher, nameRegex))
                        {
                            return true;
                        }
                    }
                }
            }
        }
    }

    return false;
}

void debug(string msg)
{
    if (options.debug)
    {
        time_t now;
        time(&now);
        char buf[sizeof "0000-00-00T00:00:00Z"];
        // strftime(buf, sizeof buf, "%FT%FZ", gmtime(&now));
        strftime(buf, sizeof buf, "%Y-%m-%dT%H:%M:%SZ", gmtime(&now));
        cout << buf << " DEBUG: " << msg << endl;
    }
}

bool in_array(const string &value, const vector<string> &array)
{
    return find(array.begin(), array.end(), value) != array.end();
}

void parseArgs(int argc, char **argv)
{
    smatch matcher;
    regex usageRegex("--usage-sleep=(\\d+)");
    regex updateRegex("--update-sleep=(\\d+)");

    for (int i = 0; i < argc; i++)
    {
        string carg = string(argv[i]);
        if (carg == "-h" || carg == "--help")
        {
            options.printHelp = true;
        }
        if (carg == "-v" || carg == "--version")
        {
            options.printVersion = true;
        }
        if (carg == "-f" || carg == "--ignore-discord")
        {
            options.ignoreDiscord = true;
        }
        if (carg == "--debug")
        {
            options.debug = true;
        }
        if (regex_search(carg, matcher, usageRegex))
        {
            options.usageSleep = stoi(matcher[1]);
        }
        if (regex_search(carg, matcher, updateRegex))
        {
            options.updateSleep = stoi(matcher[1]);
        }
    }
}

string getDistro()
{
    string distro;
    string line;
    ifstream lsbrelease;
    regex distroreg("DISTRIB_ID=\"?([a-zA-Z0-9 ]+)\"?");
    smatch distromatcher;
    lsbrelease.open("/etc/lsb-release");
    while (getline(lsbrelease, line))
    {
        if (regex_search(line, distromatcher, distroreg))
        {
            distro = distromatcher[1];
        }
    }
    return distro;
}

WindowAsset getWindowAsset(string w)
{
    WindowAsset window{};
    window.text = w;
    if(w == "") {
        window.image = "";
        return window;
    }
    window.image = "file";
    w = lower(w);

    if (in_array(w, apps))
    {
        window.image = w;
    }
    else
    {
        for (const auto &kv : aliases)
        {
            regex r = regex(kv.first);
            smatch m;
            if (regex_match(w, m, r))
            {
                window.image = kv.second;
                break;
            }
        }
    }

    return window;
}

DistroAsset getDistroAsset(string d)
{
    DistroAsset dist{};
    dist.text = d;
    dist.image = "tux";

    for (const auto &kv : distros)
    {
        regex r = regex(kv.first);
        smatch m;
        if (regex_match(d, m, r))
        {
            dist.image = kv.second;
            break;
        }
    }

    return dist;
}
