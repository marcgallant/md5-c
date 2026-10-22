#include <fcntl.h>
#include <string.h>
#include <stdio.h>
#include <unistd.h>
#include <errno.h>
#include <stdint.h>

#define BUF_SIZE 4096
#define U32_MASK 0xffffffff

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

uint32_t a0 = 0x67452301;
uint32_t b0 = 0xefcdab89;
uint32_t c0 = 0x98badcfe;
uint32_t d0 = 0x10325476;

uint8_t block[64] = { 0 };

uint64_t block_size = 0;
uint64_t byte_count = 0;

void process_block(void) {
    uint32_t M[16];
	for (int i = 0; i < 16; i++) {
		int j = i * 4;
		M[i] = block[j]
            | block[j + 1] << 8
            | block[j + 2] << 16 
            | block[j + 3] << 24;
    }
	uint32_t A = a0;
	uint32_t B = b0;
	uint32_t C = c0;
	uint32_t D = d0;

    for (int i = 0; i < 64; i++) {
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
    
    a0 += A;
    b0 += B;
    c0 += C;
    d0 += D;
}

void process_byte(uint8_t c) {
    //printf("process_byte: %c\n", c);

    block[block_size++] = c;
    if (block_size == 64) {
        process_block();
        block_size = 0;
    }
}

int process_input(int fd, char *filename) {

    while (1) {
        uint8_t buf[64 * 4096];
        ssize_t n = read(fd, buf, sizeof(buf));
        if (n == -1) {
			switch (errno) {
			case EINTR:
				continue;
			default:
                printf("md5sum: %s: %s", filename, strerror(errno));
				return -1;
			}
		} else if (n == 0) {
            break;
        }

        for (int i = 0; i < n; i++) {
            uint8_t byte = buf[i];
			process_byte(byte);
			byte_count++;
        }
    }

    process_byte(0x80);
    while (block_size != 56 ){
        process_byte(0);
    }

	uint64_t bits_count = byte_count << 3;

	for (int i = 0; i < 8; i++) {
        uint8_t byte = bits_count >> (i * 8);
		process_byte(byte);
	}

	if (block_size != 0)
		return -1;

    return 0;
}

void print_hash(char *filename) {
    printf("a0=%u b0=%u c0=%u d0=%u\n", a0, b0, c0, d0);

    uint32_t words[] = { a0, b0, c0, d0 };

    for (int i = 0; i < 4; i++) {
		uint32_t word = words[i];

		for (int j = 0; j < 4; j++) {
			uint8_t byte = word >> (j*8);
			printf("%02x", byte);
		}
	}

    printf("  %s\n", filename);
}

int main(int argc, char *argv[]) {
    
    char *filename;
    if (argc > 1 && strcmp(filename = argv[1], "-") != 0) {
        int fd = open(filename, O_RDONLY);

        if (fd == -1) {
            printf("md5sum: %s: %s\n", filename, strerror(errno));
            return 1;
        }

        int ret = process_input(fd, filename);
        if (ret == -1)
            return ret;

        print_hash(filename);
    }

    return 0;
}
