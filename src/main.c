/* -*- Mode: c; tab-width: 4; c-basic-offset: 4; indent-tabs-mode: nil; -*- */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <sys/types.h>
#include <sys/stat.h>
#include <sys/time.h>
#include <unistd.h>

#include "config.h"
#include "base.h"
#include "pj.h"

#ifdef USE_TERMIOS
#include <fcntl.h>
#include <termios.h>
#endif


static void PrintHead(void)
{
    printf("PiJockey version %x.%x\r\n", VERSION_MAJOR, VERSION_MINOR);
    printf("build %s\r\n", __DATE__);
}

static void PrintCommandUsage(void)
{
    printf("usage: pj [options] <layer0.glsl> [layer1.glsl] [layer2.glsl] ...\r\n");
    printf("options:\r\n");
    printf("  help:\r\n");
    printf("    --help, -h     print this help usage\r\n");
    printf("  debug:\r\n");
    printf("    --debug        enable debug printing\r\n");
    printf("  offscreen format:\r\n");
    printf("    --RGB888\r\n");
    printf("    --RGBA8888 (default)\r\n");
    printf("    --RGB565\r\n");
    printf("    --RGBA4444\r\n");
    printf("  interpolation mode:\r\n");
    printf("    --nearestneighbor (default)\r\n");
    printf("    --bilinear\r\n");
    printf("  wrap mode:\r\n");
    printf("    --wrap-clamp_to_edge\r\n");
    printf("    --wrap-repeat (default)\r\n");
    printf("    --wrap-mirror_repeat\r\n");
    printf("  backbuffer:\r\n");
    printf("    --backbuffer   enable backbuffer (default:OFF)\r\n");
    printf("  network:\r\n");
    printf("    --net	enable network (default:OFF)\r\n");
    printf("    --tcp	enable TCP (default:UDP)\r\n");
    printf("    --port	listen on port (default:6666)\r\n");
    printf("    --params	number of net input params (default:0)\r\n");
    printf("\r\n");
}

int main(int argc, char *argv[])
{
    int ret;
    int i;

    /* Line-buffering / unbuffered output for live terminal and service monitoring */
    setvbuf(stdout, NULL, _IONBF, 0);
    setvbuf(stderr, NULL, _IONBF, 0);

    /* Check for help flag or no arguments before initializing display / raw mode */
    for (i = 1; i < argc; i++) {
        if (strcmp(argv[i], "--help") == 0 || strcmp(argv[i], "-h") == 0 ||
            strcmp(argv[i], "-help") == 0 || strcmp(argv[i], "help") == 0) {
            PrintHead();
            PrintCommandUsage();
            return EXIT_SUCCESS;
        }
    }

    if (argc == 1) {
        PrintHead();
        PrintCommandUsage();
        return EXIT_SUCCESS;
    }

    // Ensure DISPLAY and XAUTHORITY default to local display :0 if not set in environment
    if (!getenv("DISPLAY") || getenv("DISPLAY")[0] == '\0') {
        setenv("DISPLAY", ":0", 0);
    }
    if (!getenv("XAUTHORITY")) {
        const char *home = getenv("HOME");
        if (home) {
            char auth[512];
            snprintf(auth, sizeof(auth), "%s/.Xauthority", home);
            if (access(auth, R_OK) == 0) {
                setenv("XAUTHORITY", auth, 0);
            }
        }
    }

#ifdef USE_TERMIOS
    struct termios ios_old, ios_new;
    int is_tty = isatty(STDIN_FILENO);

    if (is_tty) {
        tcgetattr(STDIN_FILENO, &ios_old);
        tcgetattr(STDIN_FILENO, &ios_new);
        cfmakeraw(&ios_new);
        tcsetattr(STDIN_FILENO, 0, &ios_new);
    }
    fcntl(0, F_SETFL, O_NONBLOCK);
#endif

    PJContext_HostInitialize();

    PrintHead();
    ret = EXIT_SUCCESS;

    PJContext *pj = malloc(PJContext_InstanceSize());
    if (!pj) {
        fprintf(stderr, "Out of memory allocating PJContext\r\n");
        PJContext_HostDeinitialize();
        return EXIT_FAILURE;
    }
    if (PJContext_Construct(pj) != 0) {
        free(pj);
        PJContext_HostDeinitialize();
        return EXIT_FAILURE;
    }
    if (PJContext_ParseArgs(pj, argc, (const char **)argv) == 0) {
        ret = PJContext_Main(pj);
    }
    PJContext_Destruct(pj);
    free(pj);

    PJContext_HostDeinitialize();

#ifdef USE_TERMIOS
    if (is_tty) {
        tcsetattr(STDIN_FILENO, 0, &ios_old);
    }
#endif
    return ret;
}

