all:
	gcc pingpong.c -o pingpong -lncurses -pthread

clean:
	rm -f pingpong
