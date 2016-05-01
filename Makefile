CC = gcc
CFLAGS += -Wall -fPIE -fstack-protector-all -D TB_NO_HW_POP_COUNT -O3
LDFLAGS += -Wl,-z,now -Wl,-z,relro -levent -lgtb -pthread

fathom: main.o tbprobe.o
	$(CXX) -o $@ main.o tbprobe.o $(LDFLAGS)

tbprobe.o: tbcore.c tbcore.h tbprobe.c tbprobe.h
	$(CC) $(CFLAGS) -c tbprobe.c

main.o: main.c tbprobe.h
	$(CC) $(CFLAGS) -c main.c
