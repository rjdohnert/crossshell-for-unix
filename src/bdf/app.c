#include "app.h"
#include "scanner.h"
#include "view.h"

typedef struct BdfConfig {
    bool   flag_b;
    bool   flag_i;
    bool   flag_l;
    bool   flag_s;
    char*  target_fstype;
    char** paths;
    int    path_count;
} BdfConfig;

struct BdfApplication {
    IVolumeScanner* scanner;
    IBdfView*       view;
    BdfConfig       config;
};

static void PrintUsage(const char* prog) {
    fprintf(stderr, "usage: %s [ -b ] [ -i ] [ -l ] [ -s ] [ -t type | file... ]\n", prog);
}

BdfApplication* BdfApplication_Create(void) {
    BdfApplication* app = (BdfApplication*)calloc(1, sizeof(BdfApplication));
    if (!app) return NULL;
    app->scanner = Win32VolumeScanner_Create();
    app->view = HpuxBdfView_Create();
    return app;
}

void BdfApplication_Destroy(BdfApplication* app) {
    if (app) {
        if (app->scanner) app->scanner->vptr->Destroy(app->scanner);
        if (app->view) app->view->vptr->Destroy(app->view);
        free(app->config.target_fstype);
        if (app->config.paths) {
            for (int i = 0; i < app->config.path_count; ++i) {
                free(app->config.paths[i]);
            }
            free(app->config.paths);
        }
        free(app);
    }
}

int BdfApplication_Run(BdfApplication* self, int argc, char** argv) {
    SetErrorMode(SEM_FAILCRITICALERRORS | SEM_NOOPENFILEERRORBOX);
    SetThreadErrorMode(SEM_FAILCRITICALERRORS | SEM_NOOPENFILEERRORBOX, NULL);

    int arg_idx = 1;
    while (arg_idx < argc && argv[arg_idx][0] == '-' && argv[arg_idx][1] != '\0') {
        char* arg = argv[arg_idx];
        if (strcmp(arg, "--") == 0) { arg_idx++; break; }
        if (strcmp(arg, "--help") == 0) {
            PrintUsage(argv[0]);
            return 0;
        }
        if (strcmp(arg, "--version") == 0) {
            printf("bdf %s\n", BDF_VERSION);
            return 0;
        }

        for (size_t j = 1; j < strlen(arg); ++j) {
            switch (arg[j]) {
            case 'b': self->config.flag_b = true; break;
            case 'i': self->config.flag_i = true; break;
            case 'l': self->config.flag_l = true; break;
            case 's': self->config.flag_s = true; break;
            case 't':
                if (arg[j + 1] != '\0') {
                    self->config.target_fstype = SafeStrDup(&arg[j + 1]);
                    j = strlen(arg) - 1;
                } else if (arg_idx + 1 < argc) {
                    self->config.target_fstype = SafeStrDup(argv[++arg_idx]);
                } else {
                    fprintf(stderr, "bdf: option requires an argument -- t\n");
                    PrintUsage(argv[0]);
                    return 1;
                }
                break;
            default:
                fprintf(stderr, "bdf: illegal option -- %c\n", arg[j]);
                PrintUsage(argv[0]);
                return 1;
            }
        }
        arg_idx++;
    }

    if (arg_idx < argc) {
        int count = argc - arg_idx;
        self->config.paths = (char**)calloc(count, sizeof(char*));
        if (self->config.paths) {
            self->config.path_count = count;
            for (int i = 0; i < count; ++i) {
                self->config.paths[i] = SafeStrDup(argv[arg_idx + i]);
            }
        }
    }

    VolumeCollection* volumes = NULL;
    int failed_count = 0;

    if (self->config.flag_b) {
        volumes = self->scanner->vptr->ScanSwapVolumes(self->scanner, self->config.target_fstype);
    } else if (self->config.path_count > 0) {
        volumes = self->scanner->vptr->ScanPathVolumes(self->scanner,
                                                       self->config.path_count,
                                                       self->config.paths,
                                                       self->config.flag_l,
                                                       self->config.target_fstype,
                                                       &failed_count);
    } else {
        volumes = self->scanner->vptr->ScanAllVolumes(self->scanner,
                                                      self->config.flag_l,
                                                      self->config.target_fstype);
    }

    if (!volumes) return 1;

    size_t count = volumes->vptr->Count(volumes);
    if (count > 0) {
        ColumnWidths cw;
        CalculateColumnWidths(volumes, &cw);

        self->view->vptr->RenderHeader(self->view, &cw, self->config.flag_i);
        for (size_t i = 0; i < count; ++i) {
            IVolume* vol = volumes->vptr->GetAt(volumes, i);
            if (!self->config.flag_s) {
                vol->vptr->Sync(vol);
            }
            self->view->vptr->RenderEntry(self->view, vol, &cw, self->config.flag_i);
        }
    }

    volumes->vptr->Destroy(volumes);
    return (failed_count > 0) ? 1 : 0;
}
