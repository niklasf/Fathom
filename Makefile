fathom: main.c tbcore.c tbcore.h tbprobe.c tbprobe.h
	clang -Wall -D TB_NO_HW_POP_COUNT tbprobe.c main.c -levent -lgtb -pthread -o fathom
