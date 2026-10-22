CFLAGS = -fPIC -Wall -Wextra -Werror -std=c11 -I include

SRC = md5.c
OBJ = $(SRC:.c=.o)
BIN = $(SRC:.c=.out)

all: $(BIN)

$(BIN): $(OBJ)

%.o: %.c
	$(CC) $(CFLAGS) -c $< -o $@

clean:
	rm -f $(BIN) $(OBJ)

.PHONY : all clean

