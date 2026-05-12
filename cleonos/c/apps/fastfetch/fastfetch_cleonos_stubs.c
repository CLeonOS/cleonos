#include "fastfetch_cleonos_compat.h"

#include "fastfetch.h"
#include "common/FFlist.h"
#include "common/FFstrbuf.h"
#include "common/io.h"
#include "common/processing.h"
#include "detection/terminaltheme/terminaltheme.h"
#include "detection/media/media.h"
#include "logo/logo.h"
#include "modules/command/option.h"
#include "modules/cpuusage/option.h"
#include "modules/diskio/option.h"
#include "modules/netio/option.h"
#include "modules/publicip/option.h"
#include "modules/weather/option.h"

bool ffDetectTerminalTheme(FFTerminalThemeResult *result, bool forceEnv) {
    (void)forceEnv;
    if (result != (FFTerminalThemeResult *)0) {
        result->fg.dark = false;
        result->bg.dark = true;
    }
    return false;
}

const FFMediaResult *ffDetectMedia(bool saveCover) {
    (void)saveCover;
    return (const FFMediaResult *)0;
}

bool ffAppendFDBuffer(int fd, FFstrbuf *buffer) {
    char chunk[256];
    for (;;) {
        ssize_t got = read(fd, chunk, sizeof(chunk));
        if (got <= 0) {
            break;
        }
        ffStrbufAppendNS(buffer, (uint32_t)got, chunk);
    }
    return true;
}

bool ffWriteFileData(const char *path, size_t size, const void *data) {
    FILE *fp = fopen(path, "w");
    bool ok;
    if (fp == (FILE *)0) {
        return false;
    }
    ok = fwrite(data, 1U, size, fp) == size;
    fclose(fp);
    return ok;
}

void ffListFilesRecursively(const char *path, bool pretty) {
    (void)path;
    (void)pretty;
}

bool ffPathExpandEnv(const char *path, FFstrbuf *buffer) {
    ffStrbufSetS(buffer, path != (const char *)0 ? path : "");
    return true;
}

const char *ffProcessSpawn(char *const argv[], bool useStdErr, FFProcessHandle *outHandle) {
    (void)argv;
    (void)useStdErr;
    if (outHandle != (FFProcessHandle *)0) {
        outHandle->pid = 0;
        outHandle->pipeRead = -1;
    }
    return "process spawning is not available";
}

const char *ffProcessReadOutput(FFProcessHandle *handle, FFstrbuf *output) {
    (void)handle;
    (void)output;
    return "process output is not available";
}

const char *ffGetTerminalResponse(const char *request, int nParams, const char *format, ...) {
    (void)request;
    (void)nParams;
    (void)format;
    return "terminal response is not available";
}

bool ffLogoPrintImageIfExists(FFLogoType type, bool printError) {
    (void)type;
    (void)printError;
    return false;
}

void ffPrepareCPUUsage(void) {
}

void ffInitCommandOptions(FFCommandOptions *options) {
    (void)options;
}

void ffPrepareCommand(FFCommandOptions *options) {
    (void)options;
}

void ffDestroyCommandOptions(FFCommandOptions *options) {
    (void)options;
}

void ffInitDiskIOOptions(FFDiskIOOptions *options) {
    (void)options;
}

void ffPrepareDiskIO(FFDiskIOOptions *options) {
    (void)options;
}

void ffDestroyDiskIOOptions(FFDiskIOOptions *options) {
    (void)options;
}

void ffInitNetIOOptions(FFNetIOOptions *options) {
    (void)options;
}

void ffPrepareNetIO(FFNetIOOptions *options) {
    (void)options;
}

void ffDestroyNetIOOptions(FFNetIOOptions *options) {
    (void)options;
}

void ffInitPublicIpOptions(FFPublicIPOptions *options) {
    (void)options;
}

void ffPreparePublicIp(FFPublicIPOptions *options) {
    (void)options;
}

void ffDestroyPublicIpOptions(FFPublicIPOptions *options) {
    (void)options;
}

void ffInitWeatherOptions(FFWeatherOptions *options) {
    (void)options;
}

void ffPrepareWeather(FFWeatherOptions *options) {
    (void)options;
}

void ffDestroyWeatherOptions(FFWeatherOptions *options) {
    (void)options;
}

FFModuleBaseInfo ffCommandModuleInfo;
FFModuleBaseInfo ffDiskIOModuleInfo;
FFModuleBaseInfo ffNetIOModuleInfo;
FFModuleBaseInfo ffPublicIPModuleInfo;
FFModuleBaseInfo ffWeatherModuleInfo;
