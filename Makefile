fathom: main.c tbcore.c tbcore.h tbprobe.c tbprobe.h
	clang -Wall -fPIE -fstack-protector-all -D TB_NO_HW_POP_COUNT tbprobe.c main.c -Wl,-z,now -Wl,-z,relro -levent -lgtb -pthread -o fathom
