#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <limits.h>
#include <errno.h>
#include <time.h>

#ifdef _POSIX_SOURCE
#include <strings.h>
/* stricmp() is only available in MS systems */
#define stricmp(x, y) strcasecmp(x, y)
#endif

/* Do not write filesystem information */
#define OPTS_NOFS 0x1
/* Force overwrite */
#define OPTS_FORCE 0x2
/* Create .BAT file */
#define OPTS_BAT 0x4

/* Invalid usage */
#define EC_INV_USAGE 1
/* Invalid FAT type parameter exit code */
#define EC_INV_FAT 2
/* Invalid number of FATs exit code */
#define EC_INV_FATCOPIES 3
/* Invalid sectors per cluster exit code */
#define EC_INV_SPC 4
/* Invalid root directory entries exit code */
#define EC_INV_ROOTDIR 5
/* Invalid disk size exit code */
#define EC_INV_SIZE 6
/* Invalid disk chs exit code */
#define EC_INV_CHS 7
/* Invalid disk type exit code */
#define EC_INV_TYPE 8
/* File error exit code */
#define EC_FILE_ERROR 9
/* Invalid FAT size exit code */
#define EC_INV_FATSIZE 10
/* Invalid cluster count exit code */
#define EC_INV_CLUSTERS 11

/* Hard Disk max cylinders */
#define HD_CYL_MAX 1023
/* Hard Disk max heads */
#define HD_HEAD_MAX 65
/* Hard Disk max sectors */
#define HD_SECT_MAX 63
/* Hard Disk media descriptor */
#define HD_MDESC 0xF8

/* FAT12 filesystem */
#define FS_FAT12 12
/* FAT16 filesystem */
#define FS_FAT16 16
/* Size of reserved area in sectors */
#define FS_RSV_SECT 1

/*
 * Examples message.
 */
const char *examples = "Some usage examples of IMGMAKE:\n\n"
"  \033[32;1mIMGMAKE -t fd\033[0m                  - create a 1.44MB floppy image \033[33;1mIMGMAKE.IMG\033[0m\n"
"  \033[32;1mIMGMAKE -t fd_1440 -force\033[0m      - force to create a floppy image \033[33;1mIMGMAKE.IMG\033[0m\n"
"  \033[32;1mIMGMAKE dos.img -t fd_2880\033[0m     - create a 2.88MB floppy image named dos.img\n"
"  \033[32;1mIMGMAKE c:\\disk.img -t hd -size 50\033[0m      - create a 50MB HDD image c:\\disk.img\n"
"  \033[32;1mIMGMAKE c:\\disk.img -t hd_520 -nofs\033[0m     - create a 520MB blank HDD image\n"
"  \033[32;1mIMGMAKE c:\\disk.img -t hd -chs 65,2,17\033[0m  - create a HDD image of specified CHS\n";

/*
 * Usage message.
 */
const char *usage = "Creates floppy or hard disk images.\n"
"Usage: \033[34;1mIMGMAKE [-?] [file] [-t type] [[-size size] | [-chs geometry]] [-spc]\033[0m\n"
"  \033[34;1m[-label label] [-nofs] [-bat] [-fs] [-fatcp] [-rootdir] [-force] [-examples]"
"\033[0m\n"
"  file: Image file to create (or \033[33;1mIMGMAKE.IMG\033[0m if not set)\n"
"  -t: Type of image.\n"
"    \033[33;1mFloppy disk templates\033[0m (names resolve to floppy sizes in KB or fd=fd_1440):\n"
"     fd_160 fd_180 fd_200 fd_320 fd_360 fd_400 fd_720 fd_1200 fd_1440 fd_2880\n"
"    \033[33;1mHard disk templates:\033[0m\n"
"     hd_250: 250MB image, hd_520: 520MB image\n"
"     hd_1gig: 1GB image, hd_2gig: 2GB image\n"
"     hd_st251: 40MB image, hd_st225: 20MB image (geometry from old drives)\n"
"    \033[33;1mCustom hard disk images:\033[0m hd (requires -size or -chs)\n"
"     -size: Size of a custom hard disk image in MB.\n"
"     -chs: Disk geometry in c(1-1023),h(1-65),s(1-63).\n"
"  -nofs: Add this parameter if a blank image should be created.\n"
"  -force: Force to overwrite the existing image file.\n"
"  -bat: Create a .bat file with the IMGMOUNT command required for this image.\n"
"  -fs: FAT filesystem type (12 or 16).\n"
"  -spc: Sectors per cluster override. Must be a power of 2.\n"
"  -fatcp: Override number of FAT table copies.\n"
"  -label: Volume label (max 11 characters).\n"
"  -rootdir: Size of root directory in entries.\n"
"  \033[32;1m-examples: Show some usage examples.\033[0m\n";

/*
 * Blank MBR with the FreeDOS bootstrap code.
 */
const unsigned char mbr[] = {
        0x33, 0xC0, 0x8E, 0xC0, 0x8E, 0xD8, 0x8E, 0xD0,
        0xBC, 0x00, 0x7C, 0xFC, 0x8B, 0xF4, 0xBF, 0x00,
        0x06, 0xB9, 0x00, 0x01, 0xF2, 0xA5, 0xEA, 0x67,
        0x06, 0x00, 0x00, 0x8B, 0xD5, 0x58, 0xA2, 0x4F,
        0x07, 0x3C, 0x35, 0x74, 0x23, 0xB4, 0x10, 0xF6,
        0xE4, 0x05, 0xAE, 0x04, 0x8B, 0xF0, 0x80, 0x7C,
        0x04, 0x00, 0x74, 0x44, 0x80, 0x7C, 0x04, 0x05,
        0x74, 0x3E, 0xC6, 0x04, 0x80, 0xE8, 0xDA, 0x00,
        0x8A, 0x74, 0x01, 0x8B, 0x4C, 0x02, 0xEB, 0x08,
        0xE8, 0xCF, 0x00, 0xB9, 0x01, 0x00, 0x32, 0xD1,
        0xBB, 0x00, 0x7C, 0xB8, 0x01, 0x02, 0xCD, 0x13,
        0x72, 0x1E, 0x81, 0xBF, 0xFE, 0x01, 0x55, 0xAA,
        0x75, 0x16, 0xEA, 0x00, 0x7C, 0x00, 0x00, 0x80,
        0xFA, 0x81, 0x74, 0x02, 0xB2, 0x80, 0x8B, 0xEA,
        0x42, 0x80, 0xF2, 0xB3, 0x88, 0x16, 0x41, 0x07,
        0xBF, 0xBE, 0x07, 0xB9, 0x04, 0x00, 0xC6, 0x06,
        0x34, 0x07, 0x31, 0x32, 0xF6, 0x88, 0x2D, 0x8A,
        0x45, 0x04, 0x3C, 0x00, 0x74, 0x23, 0x3C, 0x05,
        0x74, 0x1F, 0xFE, 0xC6, 0xBE, 0x31, 0x07, 0xE8,
        0x71, 0x00, 0xBE, 0x4F, 0x07, 0x46, 0x46, 0x8B,
        0x1C, 0x0A, 0xFF, 0x74, 0x05, 0x32, 0x7D, 0x04,
        0x75, 0xF3, 0x8D, 0xB7, 0x7B, 0x07, 0xE8, 0x5A,
        0x00, 0x83, 0xC7, 0x10, 0xFE, 0x06, 0x34, 0x07,
        0xE2, 0xCB, 0x80, 0x3E, 0x75, 0x04, 0x02, 0x74,
        0x0B, 0xBE, 0x42, 0x07, 0x0A, 0xF6, 0x75, 0x0A,
        0xCD, 0x18, 0xEB, 0xAC, 0xBE, 0x31, 0x07, 0xE8,
        0x39, 0x00, 0xE8, 0x36, 0x00, 0x32, 0xE4, 0xCD,
        0x1A, 0x8B, 0xDA, 0x83, 0xC3, 0x60, 0xB4, 0x01,
        0xCD, 0x16, 0xB4, 0x00, 0x75, 0x0B, 0xCD, 0x1A,
        0x3B, 0xD3, 0x72, 0xF2, 0xA0, 0x4F, 0x07, 0xEB,
        0x0A, 0xCD, 0x16, 0x8A, 0xC4, 0x3C, 0x1C, 0x74,
        0xF3, 0x04, 0xF6, 0x3C, 0x31, 0x72, 0xD6, 0x3C,
        0x35, 0x77, 0xD2, 0x50, 0xBE, 0x2F, 0x07, 0xBB,
        0x1B, 0x06, 0x53, 0xFC, 0xAC, 0x50, 0x24, 0x7F,
        0xB4, 0x0E, 0xCD, 0x10, 0x58, 0xA8, 0x80, 0x74,
        0xF2, 0xC3, 0x56, 0xB8, 0x01, 0x03, 0xBB, 0x00,
        0x06, 0xB9, 0x01, 0x00, 0x32, 0xF6, 0xCD, 0x13,
        0x5E, 0xC6, 0x06, 0x4F, 0x07, 0x3F, 0xC3, 0x0D,
        0x8A, 0x0D, 0x0A, 0x46, 0x35, 0x20, 0x2E, 0x20,
        0x2E, 0x20, 0x2E, 0xA0, 0x64, 0x69, 0x73, 0x6B,
        0x20, 0x32, 0x0D, 0x0A, 0x0A, 0x44, 0x65, 0x66,
        0x61, 0x75, 0x6C, 0x74, 0x3A, 0x20, 0x46, 0x31,
        0xA0, 0x00, 0x01, 0x00, 0x04, 0x00, 0x06, 0x03,
        0x07, 0x07, 0x0A, 0x0A, 0x63, 0x0E, 0x64, 0x0E,
        0x65, 0x14, 0x80, 0x19, 0x81, 0x19, 0x82, 0x19,
        0x83, 0x1E, 0x93, 0x24, 0xA5, 0x2B, 0x9F, 0x2F,
        0x75, 0x33, 0x52, 0x33, 0xDB, 0x36, 0x40, 0x3B,
        0xF2, 0x41, 0x00, 0x44, 0x6F, 0xF3, 0x48, 0x70,
        0x66, 0xF3, 0x4F, 0x73, 0xB2, 0x55, 0x6E, 0x69,
        0xF8, 0x4E, 0x6F, 0x76, 0x65, 0x6C, 0xEC, 0x4D,
        0x69, 0x6E, 0x69, 0xF8, 0x4C, 0x69, 0x6E, 0x75,
        0xF8, 0x41, 0x6D, 0x6F, 0x65, 0x62, 0xE1, 0x46,
        0x72, 0x65, 0x65, 0x42, 0x53, 0xC4, 0x42, 0x53,
        0x44, 0xE9, 0x50, 0x63, 0x69, 0xF8, 0x43, 0x70,
        0xED, 0x56, 0x65, 0x6E, 0x69, 0xF8, 0x44, 0x6F,
        0x73, 0x73, 0x65, 0xE3, 0x3F, 0xBF, 0x00, 0x00,
        0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
        0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
        0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
        0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
        0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
        0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
        0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
        0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x55, 0xAA
};

/*
 * Program options.
 */
typedef struct {
    const char *filename; /* Image filename */
    const char *type;     /* Image type */
    const char *label;    /* Image volume label */
    int size;             /* Image size in MiB */
    int c;                /* Image cylinders */
    int h;                /* Image heads */
    int s;                /* Image sectors */
    int spc;              /* Image sectors per cluster */
    int fatcopies;        /* Number of FAT copies of image */
    int rootdir;          /* Number of root directory entries of image */
    int fat;              /* Image filesystem type */
    int flags;            /* Program flags */
} options;

/**
 * Filesystem label.
 */
typedef struct {
    const char *text; /* Label text */
    size_t len;       /* Label text length */
} label;

/*
 * Filesystem specification.
 */
typedef struct {
    int type;       /* File system type */
    int spc;        /* Sectors per cluster */
    int rtent;      /* Root entries */
    int mdesc;      /* Media descriptor */
    int fatnum;     /* Number of FATs */
    long fatsize;   /* Size of each FAT in sectors */
    long voff;      /* Volume offset in sectors */
    long vsize;     /* Volume size in sectors */
    label *vlabel;  /* Volume label */
} fsspec;

/*
 * Image specification.
 */
typedef struct {
    int cylinders; /* Disk cylinders */
    int heads;     /* Disk heads */
    int sectors;   /* Disk sectors */
    fsspec *fs;    /* Filesystem specification, can be NULL */
} imgspec;

/*
 * Alphanumeric to integer with error checking.
 */
int atois(const char *str, int *val) {
    char *rest;
    long conv;

    errno = 0;
    conv = strtol(str, &rest, 10);
    if (errno == ERANGE || *rest != '\0' || str == rest) {
        return 1;
    }

#if LONG_MAX != INT_MAX
    if (conv < INT_MIN || conv > INT_MAX)
        return 1;
#endif

    *val = (int) conv;
    return 0;
}

/*
 * Copies a word (2 bytes) into the destination memory address.
 */
void *memcpyw(void *dest, int word) {
    ((char *) dest)[0] = (char) (word & 0x00FF);
    ((char *) dest)[1] = (char) ((word & 0xFF00) >> 8);
    return dest;
}

/*
 * Copies a double word (4 bytes) into the destination memory address.
 */
void *memcpydw(void *dest, long dword) {
    ((char *) dest)[0] = (char) (dword & 0x000000FFL);
    ((char *) dest)[1] = (char) ((dword & 0x0000FF00L) >> 8);
    ((char *) dest)[2] = (char) ((dword & 0x00FF0000L) >> 16);
    ((char *) dest)[3] = (char) ((dword & 0xFF000000L) >> 24);
    return dest;
}

void options_parse(options *opts, const int argc, const char **argv) {
    int i;
    char val[16], *tok;
    /* skip the first argument, it is the filename */
    for (i = 1; i < argc; i++) {
        errno = 0;
        if (stricmp(argv[i], "-?") == 0) {
            fputs(usage, stdout);
            exit(0);
        } else if (stricmp(argv[i], "-examples") == 0) {
            fputs(examples, stdout);
            exit(0);
        } else if (stricmp(argv[i], "-t") == 0) {
            opts->type = argv[++i];
        } else if (stricmp(argv[i], "-size") == 0) {
            if (atois(argv[++i], &opts->size) != 0) {
                fputs("Invalid -size option. Unrecognized value format.", stderr);
                exit(EC_INV_SIZE);
            }
        } else if (stricmp(argv[i], "-chs") == 0) {
            strncpy(val, argv[++i], sizeof(val));
            tok = strtok(val, ",");
            if (tok == NULL || atois(tok, &opts->c) != 0) {
                fputs("Invalid -chs option. Unrecognized value format.", stderr);
                exit(EC_INV_CHS);
            }
            tok = strtok(NULL, ",");
            if (tok == NULL || atois(tok, &opts->h) != 0) {
                fputs("Invalid -chs option. Unrecognized value format.", stderr);
                exit(EC_INV_CHS);
            }
            tok = strtok(NULL, ",");
            if (tok == NULL || atois(tok, &opts->s) != 0) {
                fputs("Invalid -chs option. Unrecognized value format.", stderr);
                exit(EC_INV_CHS);
            }
        } else if (stricmp(argv[i], "-spc") == 0) {
            if (atois(argv[++i], &opts->spc) != 0) {
                fputs("Invalid -spc option. Unrecognized value format.", stderr);
                exit(EC_INV_SPC);
            }
        } else if (stricmp(argv[i], "-nofs") == 0) {
            opts->flags |= OPTS_NOFS;
        } else if (stricmp(argv[i], "-bat") == 0) {
            opts->flags |= OPTS_BAT;
        } else if (stricmp(argv[i], "-fs") == 0) {
            if (atois(argv[++i], &opts->fat) != 0) {
                fputs("Invalid -fs option. Unrecognized value format.", stderr);
                exit(EC_INV_FAT);
            }
        } else if (stricmp(argv[i], "-fatcopies") == 0) {
            if (atois(argv[++i], &opts->fatcopies) != 0) {
                fputs("Invalid -fatcopies option. Unrecognized value format.", stderr);
                exit(EC_INV_FATCOPIES);
            }
        } else if (stricmp(argv[i], "-rootdir") == 0) {
            if (atois(argv[++i], &opts->rootdir) != 0) {
                fputs("Invalid -rootdir option. Unrecognized value format.", stderr);
                exit(EC_INV_ROOTDIR);
            }
        } else if (stricmp(argv[i], "-force") == 0) {
            opts->flags |= OPTS_FORCE;
        } else if (stricmp(argv[i], "-label") == 0) {
            opts->label = argv[++i];
        } else if (opts->filename == NULL) {
            opts->filename = argv[i];
        };
    }
}

void options_tocustomchs(const options *opts, imgspec *img) {
    /* prioritize -size when both -size and -chs are set */
    if (opts->size >= 0) {
        /*
         * chs = size in B / 512
         *     = size in MiB * 1024 * 1024 / 512
         *     = size in MiB * 2048
         */
        long target_chs = opts->size * 2048L;
        int eff_size;

        if (opts->size < 3 || opts->size > 2014) {
            fputs("Invalid -size option. Must be between 3 and 2014 MiB.", stderr);
            exit(EC_INV_SIZE);
        }

        /* Figure out good CHS values */
        img->heads = 2;
        while ((long) img->heads * HD_SECT_MAX * HD_CYL_MAX < target_chs)
            img->heads <<= 1;
        if (img->heads > HD_HEAD_MAX)
            img->heads = HD_HEAD_MAX;

        img->sectors = 8;
        while ((long) img->heads * img->sectors * HD_CYL_MAX < target_chs)
            img->sectors <<= 1;
        if (img->sectors > HD_SECT_MAX)
            img->sectors = HD_SECT_MAX;

        img->cylinders = (int) (target_chs / (img->heads * img->sectors));
        if (img->cylinders > HD_CYL_MAX)
            img->cylinders = HD_CYL_MAX;

        eff_size = (int) ((long) img->cylinders * img->heads * img->sectors /
                          2048L);
        if (opts->size != eff_size) {
            fprintf(stderr, "Warning: effective image size will be %d MiB.\n", eff_size);
        }

    } else if (opts->c >= 0 && opts->h >= 0 && opts->s >= 0) {
        if (opts->c > HD_CYL_MAX) {
            fprintf(stderr,"Invalid -chs option. Cylinders must be between 1 and %d.", HD_CYL_MAX);
            exit(EC_INV_CHS);
        }
        if (opts->h > HD_HEAD_MAX) {
            fprintf(stderr, "Invalid -chs option. Heads must be between 1 and %d.", HD_HEAD_MAX);
            exit(EC_INV_CHS);
        }
        if (opts->s > HD_SECT_MAX) {
            fprintf(stderr, "Invalid -chs option. Sectors must be between 1 and %d.", HD_SECT_MAX);
            exit(EC_INV_CHS);
        }
        if (opts->c * opts->h * opts->s < 6144L /* 3 MiB */) {
            fputs("Invalid -chs option. The provided geometry specifies a disk that is smaller than 3 MiB.", stderr);
            exit(EC_INV_CHS);
        }
        img->cylinders = opts->c;
        img->heads = opts->h;
        img->sectors = opts->s;
    } else {
        fputs("You must specify a valid -size or -chs when using type \"hd\".", stderr);
        exit(EC_INV_TYPE);
    }
}

void options_tofsspec(const options *opts, imgspec *img) {
    if (opts->flags & OPTS_NOFS) {
        img->fs = NULL;
    } else {
        const long chs = (long) img->cylinders * img->heads * img->sectors;
        long clusters, max_clusters, min_clusters;
        long eff_vsize;
        fsspec *fs = img->fs;

        /* copy the label even if it is NULL */
        if (opts->label == NULL) {
            fs->vlabel = NULL;
        } else {
            fs->vlabel->text = opts->label;
            fs->vlabel->len = strlen(opts->label);

            if (fs->vlabel->len > 11) {
                fputs("Warning: provided label is too long, truncating to 11 characters.", stderr);
                fs->vlabel->len = 11;
            }
        }

        /* volume offset and size (in sectors) */
        fs->voff = fs->mdesc == HD_MDESC ? img->sectors : 0L;
        fs->vsize = chs - fs->voff;

        if (opts->fat >= 0) {
            if (opts->fat != FS_FAT12 && opts->fat != FS_FAT16) {
                fputs("Invalid -fat option. Must be 12 or 16.", stderr);
                exit(EC_INV_FAT);
            }
            if (opts->fat == FS_FAT12 && fs->vsize >= 65536L /* 32 MiB */) {
                fputs("Invalid -fat option. Disk is too large for FAT12.", stderr);
                exit(EC_INV_FAT);
            }
            fs->type = opts->fat;
        } else {
            fs->type = fs->vsize >= 24576L /* 12 MiB */ ? FS_FAT16 : FS_FAT12;
        }

        if (fs->type == FS_FAT12) {
            max_clusters = 0x0FF6L;
            min_clusters = 0L;
        } else {
            max_clusters = 0xFFF6L;
            min_clusters = 0x0FF6L;
        }

        if (opts->fatcopies >= 0) {
            if (opts->fatcopies < 1 || opts->fatcopies > 4) {
                fputs("Invalid -fatcopies option, must be between 1 and 4.", stderr);
                exit(EC_INV_FATCOPIES);
            }
            fs->fatnum = opts->fatcopies;
        } else {
            fs->fatnum = 2;
        }

        if (opts->spc >= 0) {
            if (opts->spc < 1 || opts->spc > 128) {
                fputs("Invalid -spc option, must be between 1 and 128.", stderr);
                exit(EC_INV_SPC);
            }
            if ((opts->spc & (opts->spc - 1)) != 0) {
                fputs("Invalid -spc option, must be a power of 2.", stderr);
                exit(EC_INV_SPC);
            }
            fs->spc = opts->spc;
        } else {
            if (chs >= 1048576L /* 512 MiB */)
                fs->spc = 4;
            else if (chs >= 131072L /* 64 MiB */)
                fs->spc = 2;
            else
                fs->spc = 1;
        }

        /* SPC just enough that we don't use more clusters than possible */
        while (fs->vsize >= fs->spc * (max_clusters - 2L) && fs->spc < 128)
            fs->spc <<= 1;

        fs->fatsize = fs->type == FS_FAT12
                      ? ((fs->vsize / fs->spc + 1L) * 3L / 2L + 511L) / 512L
                      : (fs->vsize / fs->spc * 2L + 511L) / 512L;

        if (fs->fatsize > 65536L) {
            fputs("Error: Generated filesystem has more than 64KB sectors per FAT.\n", stderr);
            exit(EC_INV_FATSIZE);
        }

        /*
         * Effective volume size in sectors without:
         * - Reserved sectors (always 1)
         * - FAT copies area
         * - Root filesystem entries area
         */
        eff_vsize = fs->vsize - FS_RSV_SECT - (fs->fatsize * fs->fatnum)
                    - ((fs->rtent * 32L) + 511L) / 512L;
        clusters = eff_vsize / fs->spc + 2L;

        if (clusters < min_clusters) {
            fputs("Error: Generated filesystem has too few clusters given the parameters.\n", stderr);
            exit(EC_INV_CLUSTERS);
        }

        if (clusters > max_clusters) {
            fputs("Error: Cluster count is too high given the volume size.\n", stderr);
            exit(EC_INV_CLUSTERS);
        }

        /* if not overridden here, rtent should be already set */
        if (opts->rootdir >= 0) {
            if (opts->rootdir < 1 || opts->rootdir > 4096) {
                fputs("Invalid -rootdir option, must be between 1 and 4096.", stderr);
                exit(EC_INV_ROOTDIR);
            }
            img->fs->rtent = opts->rootdir;
        }
    }
}

void options_toimgspec(const options *opts, imgspec *img) {
    /* hard disk defaults */
    img->fs->mdesc = HD_MDESC;
    img->fs->rtent = 512;

    if (stricmp(opts->type, "fd_160") == 0) {
        img->fs->mdesc = 0xFE;
        img->fs->rtent = 56;
        img->cylinders = 40;
        img->heads = 1;
        img->sectors = 8;
    } else if (stricmp(opts->type, "fd_180") == 0) {
        img->fs->mdesc = 0xFC;
        img->fs->rtent = 56;
        img->cylinders = 40;
        img->heads = 1;
        img->sectors = 9;
    } else if (stricmp(opts->type, "fd_200") == 0) {
        img->fs->mdesc = 0xFC;
        img->fs->rtent = 56;
        img->cylinders = 40;
        img->heads = 1;
        img->sectors = 10;
    } else if (stricmp(opts->type, "fd_320") == 0) {
        img->fs->mdesc = 0xFF;
        img->fs->rtent = 112;
        img->cylinders = 40;
        img->heads = 2;
        img->sectors = 8;
    } else if (stricmp(opts->type, "fd_360") == 0) {
        img->fs->mdesc = 0xFD;
        img->fs->rtent = 112;
        img->cylinders = 40;
        img->heads = 2;
        img->sectors = 9;
    } else if (stricmp(opts->type, "fd_400") == 0) {
        img->fs->mdesc = 0xFD;
        img->fs->rtent = 112;
        img->cylinders = 40;
        img->heads = 2;
        img->sectors = 10;
    } else if (stricmp(opts->type, "fd_720") == 0) {
        img->fs->mdesc = 0xF9;
        img->fs->rtent = 112;
        img->cylinders = 80;
        img->heads = 2;
        img->sectors = 9;
    } else if (stricmp(opts->type, "fd_1200") == 0) {
        img->fs->mdesc = 0xF9;
        img->fs->rtent = 224;
        img->cylinders = 80;
        img->heads = 2;
        img->sectors = 15;
    } else if (stricmp(opts->type, "fd_1440") == 0 ||
               stricmp(opts->type, "fd") == 0 ||
               stricmp(opts->type, "floppy") == 0) {
        img->fs->mdesc = 0xF0;
        img->fs->rtent = 224;
        img->cylinders = 80;
        img->heads = 2;
        img->sectors = 18;
    } else if (stricmp(opts->type, "fd_2880") == 0) {
        img->fs->mdesc = 0xF0;
        img->fs->rtent = 512;
        img->cylinders = 80;
        img->heads = 2;
        img->sectors = 36;
    } else if (stricmp(opts->type, "hd_250") == 0) {
        img->cylinders = 489;
        img->heads = 16;
        img->sectors = 63;
    } else if (stricmp(opts->type, "hd_520") == 0) {
        img->cylinders = 1023;
        img->heads = 16;
        img->sectors = 63;
    } else if (stricmp(opts->type, "hd_1gig") == 0) {
        img->cylinders = 1023;
        img->heads = 32;
        img->sectors = 63;
    } else if (stricmp(opts->type, "hd_2gig") == 0) {
        img->cylinders = 1023;
        img->heads = 64;
        img->sectors = 63;
    } else if (stricmp(opts->type, "hd_st251") == 0) {
        img->cylinders = 820;
        img->heads = 6;
        img->sectors = 17;
    } else if (stricmp(opts->type, "hd_st225") == 0) {
        img->cylinders = 615;
        img->heads = 4;
        img->sectors = 17;
    } else if (stricmp(opts->type, "hd") == 0) {
        options_tocustomchs(opts, img);
    } else {
        fputs("Invalid -t option. Type \"imgmake -?\" for possible values.", stderr);
        exit(EC_INV_TYPE);
    }

    options_tofsspec(opts, img);
}

int imgspec_write(const imgspec *img, FILE *fp) {
    const fsspec *fs = img->fs;
    const long chs = (long) img->cylinders * img->heads * img->sectors;
    const long size = chs * 512L;
    unsigned char buf[512];
    long i;

    /* preallocate space on HDD by writing the last byte */
    if (fseek(fp, size - 1L, SEEK_SET) != 0 || fwrite("\0", 1, 1, fp) != 1) {
        fprintf(stderr, "Not enough space available for the image file. Need %ld bytes.\n", size);
        return 1;
    }

    if (fs == NULL) {
        return 0;
    }

    if (fseek(fp, 0, SEEK_SET) != 0) {
        perror("Error while accessing image file");
        return 1;
    }

    /* if it is an hard disk, write MBR */
    if (fs->mdesc == HD_MDESC) {
        /* load default MBR into buffer */
        memcpy(buf, mbr, sizeof(mbr));

        /* active partition marker */
        buf[0x1BE] = 0x80;
        /* start head: head 0 has partition table, head 1 first partition */
        buf[0x1BF] = 1;
        /* start sector with bits 8-9 of start cylinder in bits 6-7 */
        buf[0x1C0] = 1;
        /* start cylinder bits 0-7 */
        buf[0x1C1] = 0;

        /* partition type */
        if (chs < 65536L) {
            /* FAT12 (0x01), FAT16 (0x04) */
            buf[0x1C2] = fs->type == FS_FAT12 ? 0x01 : 0x04;
        } else {
            /* FAT16B (0x06) the only option when more than 65536 sectors */
            buf[0x1C2] = 0x06;
        }

        /* end head (0-based) */
        buf[0x1C3] = img->heads - 1;
        /* end sector with bits 8-9 of end cylinder (0-based) in bits 6-7 */
        buf[0x1C4] = img->sectors | (((img->cylinders - 1) & 0x300) >> 2);
        /* end cylinder (0-based) bits 0-7 */
        buf[0x1C5] = (img->cylinders - 1) & 0xFF;

        /* first absolute sector of partition 1 */
        memcpydw(buf + 0x1C6, fs->voff);
        /* sector size of partition 1 */
        memcpydw(buf + 0x1CA, fs->vsize);

        if (fwrite(buf, 1, 512, fp) != 512) {
            perror("Unable to write image file MBR.");
            return 1;
        }
    }

    /* write boot sector */
    memset(buf, 0, sizeof(buf));

    /* ML to jump to boot code */
    buf[0x000] = 0xEB;
    buf[0x001] = 0x3C;
    buf[0x002] = 0x90;

    /* OEM name */
    memcpy(buf + 0x003, "MSDOS5.0", 8);

    /* always 512 bytes per sector */
    memcpyw(buf + 0x00B, 512);

    /* sectors per cluster */
    buf[0x00D] = fs->spc;

    /* reserved sectors (always 1 for FAT12/16) */
    memcpyw(buf + 0x00E, FS_RSV_SECT);

    /* number of FATs */
    buf[0x010] = fs->fatnum;

    /* root entries */
    memcpyw(buf + 0x011, fs->rtent);

    /* total sectors in the filesystem */
    if (fs->vsize > 0xFFFFL) {
        memcpydw(buf + 0x020, fs->vsize);
    } else {
        memcpyw(buf + 0x013, (int) fs->vsize);
    }

    /* media descriptor */
    buf[0x015] = fs->mdesc;

    /* size of each FAT in sectors, always less than 2^16 for FAT12/16 */
    memcpyw(buf + 0x016, (int) fs->fatsize);

    /* geometry */
    memcpyw(buf + 0x018, img->sectors);
    memcpyw(buf + 0x01A, img->heads);

    /* sectors before the start partition */
    memcpydw(buf + 0x01C, fs->voff);

    /* BIOS INT 13h drive number (0x00 first floppy, 0x80 first hard disk) */
    if (fs->mdesc == HD_MDESC)
        buf[0x024] = 0x80;

    /* extended boot signature */
    buf[0x026] = 0x29;

    /* volume serial number */
    memcpydw(buf + 0x027, time(NULL));

    /* volume label */
    if (fs->vlabel != NULL) {
        memcpy(buf + 0x02B, fs->vlabel->text, fs->vlabel->len);
        memset(buf + 0x02B + fs->vlabel->len, ' ', 11 - fs->vlabel->len);
    } else {
        memcpy(buf + 0x02B, "NO NAME    ", 11);
    }

    /* ASCII filesystem type */
    if (fs->type == FS_FAT12) {
        memcpy(buf + 0x036, "FAT12   ", 8);
    } else {
        memcpy(buf + 0x036, "FAT16   ", 8);
    }

    /* boot sector signature */
    buf[0x1FE] = 0x55;
    buf[0x1FF] = 0xAA;

    if (fseek(fp, fs->voff * 512, SEEK_SET) != 0 ||
        fwrite(&buf, 512, 1, fp) != 1) {
        perror("Unable to write image file boot sector.\n");
        return 1;
    }

    /* write FATs */
    if (fs->type == FS_FAT16) {
        memcpydw(buf, 0xFFFFFF00L | fs->mdesc);
    } else {
        memcpydw(buf, 0x00FFFF00L | fs->mdesc);
    }

    for (i = 0; i < fs->fatnum; i++) {
        long off = (fs->voff + FS_RSV_SECT + fs->fatsize * i) * 512L;

        if (fseek(fp, off, SEEK_SET) != 0 ||
            fwrite(&buf, 4, 1, fp) != 1) {
            perror("Unable to write image file FAT.\n");
            return 1;
        }
    }

    /* create the special filesystem entry for the label */
    if (fs->vlabel != NULL) {
        long off = (fs->voff + FS_RSV_SECT + fs->fatsize * fs->fatnum) * 512L;

        memcpy(buf, fs->vlabel->text, fs->vlabel->len);
        memset(buf + fs->vlabel->len, ' ', 11 - fs->vlabel->len);
        buf[11] = 0x08;

        if (fseek(fp, off, SEEK_SET) != 0 ||
            fwrite(&buf, 12, 1, fp) != 1) {
            perror("Unable to write image file filesystem entry for volume label.\n");
            return 1;
        }
    }

    return 0;
}

int main(const int argc, const char* argv[]) {
    options opts = {NULL, NULL, NULL, -1, -1, -1, -1, -1, -1, -1, -1, 0};
    label vlabel;
    fsspec fs;
    imgspec img;
    FILE* fp;

    /* avoids malloc() */
    fs.vlabel = &vlabel;
    img.fs = &fs;

    options_parse(&opts, argc, argv);
    if (opts.type == NULL) {
        fputs(usage, stderr);
        return EC_INV_USAGE;
    }

    opts.filename = opts.filename == NULL ? "IMGMAKE.IMG" : opts.filename;
    options_toimgspec(&opts, &img);

    if (!(opts.flags & OPTS_FORCE)) {
        fp = fopen(opts.filename, "r");
        if (fp != NULL) {
            fprintf(stderr,"The file \"%s\" already exists. You can specify \"-force\" to overwrite.\n", opts.filename);
            fclose(fp);
            return EC_FILE_ERROR;
        }
    }

    fp = fopen(opts.filename, "w+");
    if (fp == NULL) {
        fprintf(stderr,"The file \"%s\" cannot be opened for writing.\n", opts.filename);
        return EC_FILE_ERROR;
    }

    fprintf(stdout, "Creating image file \"%s\" with %u cylinders, %u heads and %u sectors.\n",
           opts.filename, img.cylinders, img.heads, img.sectors);
    if (imgspec_write(&img, fp) != 0) {
        /* error messages are printed by imgspec_write */
        fclose(fp);
        remove(opts.filename);
        return EC_FILE_ERROR;
    }

    fclose(fp);

    /* write the .BAT file */
    if (opts.flags & OPTS_BAT) {
        char bat[12];
        size_t batlen = strlen(opts.filename);

        strncpy(bat, opts.filename, sizeof(bat));
        strncpy(bat + (batlen > 3 ? batlen - 4 : batlen), ".BAT", 4);

        fp = fopen(bat, "w");
        if (fp == NULL) {
            fprintf(stderr, "The file \"%s\" cannot be opened for writing.\n", bat);
            return EC_FILE_ERROR;
        }

        if (fprintf(fp, "imgmount %c %s -size 512,%d,%d,%d\r\n",
                    fs.mdesc == HD_MDESC ? 'c' : 'a', opts.filename,
                    img.cylinders, img.heads, img.sectors) < 0) {
            perror("Unable to write to .BAT file");
            fclose(fp);
            remove(bat);
            return EC_FILE_ERROR;
        }

        fclose(fp);
    }

    return 0;
}
