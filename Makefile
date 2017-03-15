# Variables
BW= bw
ENC= encoding
HUF= huffman
EXE= $(BW) $(ENC) $(HUF)

# Usual compilation flags
CFLAGS= -std=c99 -Wall -Wextra -g
CPPFLAGS= -I../include

# Special rules and targets
.PHONY: all clean help

# Rules and targets
all: $(EXE)

$(BW): $(BW).o
	$(CC) $(CFLAGS) -o $@ $^

$(BW).o: $(BW).c
	$(CC) $(CFLAGS) $(CPPFLAGS) -c $<

$(ENC): $(ENC).o
	$(CC) $(CFLAGS) -o $@ $^

$(ENC).o: $(ENC).c
	$(CC) $(CFLAGS) $(CPPFLAGS) -c $<

$(HUF): $(HUF).o
	$(CC) $(CFLAGS) -o $@ $^

$(HUF).o: $(HUF).c
	$(CC) $(CFLAGS) $(CPPFLAGS) -c $<

clean:
	@rm -f *.o $(EXE)

help:
	@echo "Usage:"
	@echo "  make [all]\t\tBuild"
	@echo "  make clean\t\tRemove all files generated by make"
	@echo "  make help\t\tDisplay this help"
