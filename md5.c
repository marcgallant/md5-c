#define _GNU_SOURCE

#include <ctype.h>
#include <errno.h>
#include <fcntl.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <getopt.h>

#define BLOCK_SIZE 64

enum {
    OPT_HELP = 10,
    OPT_VERSION
};

struct options {
    int binary;
    int check;
    int tag;
    int text;
    int zero;
    int ignore_missing;
    int quiet;
    int status;
    int strict;
    int warn;
};

static struct options opts = {0};

// s specifies the per-round shift amounts
uint32_t s[] = {
    7, 12, 17, 22,  7, 12, 17, 22,  7, 12, 17, 22,  7, 12, 17, 22,
    5,  9, 14, 20,  5,  9, 14, 20,  5,  9, 14, 20,  5,  9, 14, 20,
    4, 11, 16, 23,  4, 11, 16, 23,  4, 11, 16, 23,  4, 11, 16, 23,
    6, 10, 15, 21,  6, 10, 15, 21,  6, 10, 15, 21,  6, 10, 15, 21,
};

uint32_t K[] = {
	0xd76aa478, 0xe8c7b756, 0x242070db, 0xc1bdceee,
	0xf57c0faf, 0x4787c62a, 0xa8304613, 0xfd469501,
	0x698098d8, 0x8b44f7af, 0xffff5bb1, 0x895cd7be,
	0x6b901122, 0xfd987193, 0xa679438e, 0x49b40821,
	0xf61e2562, 0xc040b340, 0x265e5a51, 0xe9b6c7aa,
	0xd62f105d, 0x02441453, 0xd8a1e681, 0xe7d3fbc8,
	0x21e1cde6, 0xc33707d6, 0xf4d50d87, 0x455a14ed,
	0xa9e3e905, 0xfcefa3f8, 0x676f02d9, 0x8d2a4c8a,
	0xfffa3942, 0x8771f681, 0x6d9d6122, 0xfde5380c,
	0xa4beea44, 0x4bdecfa9, 0xf6bb4b60, 0xbebfbc70,
	0x289b7ec6, 0xeaa127fa, 0xd4ef3085, 0x04881d05,
	0xd9d4d039, 0xe6db99e5, 0x1fa27cf8, 0xc4ac5665,
	0xf4292244, 0x432aff97, 0xab9423a7, 0xfc93a039,
	0x655b59c3, 0x8f0ccc92, 0xffeff47d, 0x85845dd1,
	0x6fa87e4f, 0xfe2ce6e0, 0xa3014314, 0x4e0811a1,
	0xf7537e82, 0xbd3af235, 0x2ad7d2bb, 0xeb86d391,
};


// Process the message in successive 512-bit chunks
void process_block(uint8_t block[], uint32_t *a0, uint32_t *b0, uint32_t *c0, uint32_t *d0) {

    //break chunk into sixteen 32-bit words M[j], 0 ≤ j ≤ 15
    uint32_t M[16] = { 0 };

	for (int i = 0; i < 16; i++) {
        for (int j = 0; j < 4; j++) {
            M[i] |= block[i * 4 + j] << j * 8;
        }
    }

    // Initialize hash value for this chunk
	uint32_t A = *a0;
	uint32_t B = *b0;
	uint32_t C = *c0;
	uint32_t D = *d0;

    // Main loop:
    for (int i = 0; i < BLOCK_SIZE; i++) {
        uint32_t F, g;

        if (i < 16) {
			F = (B & C) | ((~B) & D);
			g = i;
        }
        else if (i < 32) {
			F = (D & B) | ((~D) & C);
			g = (5 * i + 1) % 16;
        }
        else if (i < 48) {
			F = B ^ C ^ D;
			g = (3 * i + 5) % 16;
        }
        else {
			F = C ^ (B | ~D);
			g = (7 * i) % 16;
        }

		F = F + A + K[i] + M[g];
		A = D;
		D = C;
		C = B;
		B += F << s[i] | (F >> (32 - s[i]));
    }
    
    // Add this chunk's hash to result so far:
    *a0 += A;
    *b0 += B;
    *c0 += C;
    *d0 += D;
}

int process_file(char *filename, char digest[33]) {
    uint64_t byte_count = 0;
    uint8_t block[BLOCK_SIZE];
    ssize_t n;

    uint32_t a0 = 0x67452301;
    uint32_t *a0_ptr = &a0;

    uint32_t b0 = 0xefcdab89;
    uint32_t *b0_ptr = &b0;

    uint32_t c0 = 0x98badcfe;
    uint32_t *c0_ptr = &c0;

    uint32_t d0 = 0x10325476;
    uint32_t *d0_ptr = &d0;

    int fd = 0;
    if (strcmp(filename,"-") != 0) {
        fd = open(filename, O_RDONLY);

        if (fd == -1) {
            printf("md5sum: %s: %s\n", filename, strerror(errno));
            return -1;
        }
    }
        
    while (1) {
        n = read(fd, block, BLOCK_SIZE);
        
        if (n == -1) {
			switch (errno) {
			case EINTR:
				continue;
			default:
                printf("md5sum: %s: %s", filename, strerror(errno));
                close(fd);
				return -1;
			}
		}

        byte_count += n;
        if (n < BLOCK_SIZE)
            break;

        process_block(block, a0_ptr, b0_ptr, c0_ptr, d0_ptr);
    }
    close(fd);

    // Pre-processing: adding a single 1 bit
    block[n++] = 0x80;

    if (n == BLOCK_SIZE) {
        process_block(block, a0_ptr, b0_ptr, c0_ptr, d0_ptr);
        n = 0;
    }

    // Pre-processing: padding with zeros
    // so that the message length in bytes ≡ 56 (mod 64).
    while (n != 56 ){
        block[n++] = 0;

        if (n == BLOCK_SIZE) {
            process_block(block, a0_ptr, b0_ptr, c0_ptr, d0_ptr);
            n = 0;
        }

    }

    //append original length in bits mod 2^64 to message
	uint64_t bits_count = byte_count << 3;
	for (int i = 0; i < 8; i++) {
        uint8_t byte = bits_count >> (i * 8);
        block[n + i] = byte;
	}

    process_block(block, a0_ptr, b0_ptr, c0_ptr, d0_ptr);

    uint32_t words[] = { *a0_ptr, *b0_ptr, *c0_ptr, *d0_ptr };
    int pos = 0;

    for (int i = 0; i < 4; i++) {
        uint32_t word = words[i];

        for (int j = 0; j < 4; j++) {
            uint8_t byte = (uint8_t)((word >> (j * 8)) & 0xff);
            pos += sprintf(&digest[pos], "%02x", (unsigned int)byte);
        }
    }

    return 0;
}

void print_digest(char *filename, char digest[33]) { 
    if (opts.tag)
        printf("MD5 (%s) = %s", filename, digest);
    else
        printf("%s %c%s", digest, opts.binary ? '*' : ' ', filename);

    if (!opts.zero)
        puts("");
}

struct checksum_entry {
    char digest[33];
    char *filename;
};

int parse_line(char *line, ssize_t len, struct checksum_entry *entry) {
    if (len < 35) 
        return -1;
    
    for (int i = 0; i < 32; i++) {
        if (!isxdigit(line[i]))
            return -1;
    }

    if (line[32] != ' ')
        return -1;

    if (line[33] != ' ' || line[33] != '*')
        return -1;

    if (line[34] == '\0')
        return -1;

    memcpy(entry->digest, line, 32);
    entry->digest[32] = '\0';

    entry->filename = &line[34];

    return 0;
}

int check_md5(char * filename) {

    FILE *fp = fopen(filename, "r");
    if (!fp) {
        printf("md5sum: %s: %s\n", filename, strerror(errno));
        return -1;
    }

    char * line = NULL;
    size_t len = 0;
    ssize_t read;

    size_t nlines = 0;
    size_t unformated = 0;
    size_t mismatch = 0;
    size_t missing = 0;
    while ((read = getline(&line, &len, fp)) != -1) {
        nlines++;
        struct checksum_entry *entry = { 0 };

       if (parse_line(line, read, entry)) {
           if (!opts.status) printf("md5sum: %s: %lu: improperly formatted MD5 checksum line\n", filename, nlines);
           unformated++;
       }

        char digest[33] = { 0 };
        if (process_file(entry->filename, digest) && !opts.ignore_missing) {
            if (!opts.status) printf("%s: FAILED open or read\n", entry->filename);
            missing++;
        }

        if (strcmp(digest, entry->digest) != 0) {
            if (!opts.status) printf("%s: FAILED\n", entry->filename);
            mismatch++;
        }
        else if (!opts.quiet)
            if (!opts.status) printf("%s: OK\n", entry->filename);
    }

    if (line)
        free(line);

    fclose(fp);

    if (unformated == nlines) {
        if (!opts.status) printf("md5sum: %s: no properly formatted checksum lines found\n", filename);
        return -1;
    }
    else {
        if (!opts.status) printf("md5sum: WARNING: %lu line is improperly formatted\n", unformated);
        if (!opts.strict) return -1;
    }
    
    if (mismatch != 0) {
        if (!opts.status) printf("md5sum: WARNING: %lu computed checksum did NOT match\n", mismatch);
    }

    if (missing != 0) {
        if (!opts.status) printf("md5sum: WARNING: %lu listed file could not be read\n", missing);
    }
    
    return mismatch + missing;
}

void print_help() {
    puts("Usage: md5sum [OPTION]... [FILE]...\n"
    "Print or check MD5 (128-bit) checksums.\n"
    "\n"
    "With no FILE, or when FILE is -, read standard input.\n"
    "  -b, --binary          read in binary mode\n"
    "  -c, --check           read checksums from the FILEs and check them\n"
    "      --tag             create a BSD-style checksum\n"
    "  -t, --text            read in text mode (default)\n"
    "  -z, --zero            end each output line with NUL, not newline,\n"
    "                          and disable file name escaping\n"
    "\n"
    "The following five options are useful only when verifying checksums:\n"
    "      --ignore-missing  don't fail or report status for missing files\n"
    "      --quiet           don't print OK for each successfully verified file\n"
    "      --status          don't output anything, status code shows success\n"
    "      --strict          exit non-zero for improperly formatted checksum lines\n"
    "  -w, --warn            warn about improperly formatted checksum lines\n"
    "\n"
    "      --help        display this help and exit\n"
    "      --version     output version information and exit\n"
    "\n"
    "The sums are computed as described in RFC 1321.\n"
    "When checking, the input should be a former output of this program.\n"
    "The default mode is to print a line with: checksum, a space,\n"
    "a character indicating input mode ('*' for binary, ' ' for text\n"
    "or where binary is insignificant), and name for each FILE.\n"
    "\n"
    "Note: There is no difference between binary mode and text mode on GNU systems.\n"
    "\n"
    "GNU coreutils online help: <https://www.gnu.org/software/coreutils/>\n"
    "Full documentation <https://www.gnu.org/software/coreutils/md5sum>\n"
    "or available locally via: info '(coreutils) md5sum invocation'");
}

void print_version() {
    puts("md5sum (GNU coreutils) 9.4\n"
    "Copyright (C) 2023 Free Software Foundation, Inc.\n"
    "License GPLv3+: GNU GPL version 3 or later <https://gnu.org/licenses/gpl.html>.\n"
    "This is free software: you are free to change and redistribute it.\n"
    "There is NO WARRANTY, to the extent permitted by law.\n"
    "\n"
    "Written by Ulrich Drepper, Scott Miller, and David Madore.");
}

int option_error(void) {
    puts("Try \'md5sum --help\' for more information.");
    return 1;
}

int main(int argc, char *argv[]) {

    int opt;
    static struct option long_options[] = {
        {"binary",         no_argument, NULL,                 'b'         },
        {"check",          no_argument, NULL,                 'c'         },
        {"tag",            no_argument, &opts.tag,            1           },
        {"text",           no_argument, NULL,                 't'         },
        {"zero",           no_argument, NULL,                 'z'         },
        
        {"ignore-missing", no_argument, &opts.ignore_missing, 1           },
        {"quiet",          no_argument, &opts.quiet,          1           },
        {"status",         no_argument, &opts.status,         1           },
        {"strict",         no_argument, &opts.strict,         1           },
        {"warn",           no_argument, &opts.warn,           1           },

        {"help",           no_argument, NULL,                  OPT_HELP    },
        {"version",        no_argument, NULL,                  OPT_VERSION },
    };
    int option_index = 0;

    while ((opt = getopt_long(argc, argv, "bctzw", long_options, &option_index)) != -1) {
        switch (opt) {
            case 0:
                break;
            case 'b':
                opts.binary = 1;
                break;
            case 'c':
                opts.check = 1;
                break;
            case 't':
                opts.text = 1;
                opts.binary = 0;
                break;
            case 'z':
                opts.zero = 1;
                break;
            case 'w':
                opts.warn = 1;
                break;
            case OPT_HELP:
                print_help();
                return 0;
            case OPT_VERSION:                    
                print_version();
                return 0;
            default: // Handles unknown flags
                return option_error();
        }
    }

    for (int i = 5; i < 10; i++) {
        if (long_options[i].flag && *long_options[i].flag && !opts.check) {
            printf("md5sum: the --%s option is meaningful only when verifying checksums\n", long_options[i].name);
            return option_error();
        }
    }

    if ((opts.binary || opts.text) && opts.check) {
        printf("md5sum: the --binary and --text options are meaningless when verifying checksums\n");
        return option_error();
    }

    if (opts.zero && opts.check) {
        printf("md5sum: the --zero option is not supported when verifying checksums\n");
        return option_error();
    }

    if (opts.tag && opts.check) {
        printf("md5sum: the --tag option is not supported when verifying checksums\n");
        return option_error();
    }

    if (argc == optind) {
        
        char digest[33] = { 0 };
        int ret = process_file("-", digest);
        if (ret)
            return 1;
        
        print_digest("-", digest);
    }

    for (int i = optind; i < argc; i++) {
        if (opts.check) {
            int ret = check_md5(argv[i]);
            if (ret)
                return 1;
        }
        else {
            char digest[33] = { 0 };
            int ret = process_file(argv[i], digest);
            if (ret)
                return 1;

            print_digest(argv[i], digest);
        }
    }

    return 0;
}
