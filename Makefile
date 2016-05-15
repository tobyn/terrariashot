CFLAGS = -O3 -pipe -Wall -Wextra -Werror

all:
	$(CC) $(CFLAGS) -o terrariashot terrariashot.c $(LDFLAGS)
