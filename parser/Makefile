# name of binary 
BIN = bin

LIB =
SRC =.
INC =.
ARG = 

CC  = gcc
OPT = -O0

# cmake dependency flags
DEPFLAGS = -MP -MD
FLAGS    = -Wall -Wextra -z noexecstack -g $(foreach D,$(INC),-I$(D)) $(OPT) $(DEPFLAGS)

# create list of c files using the src directories
FILES = $(foreach D,$(SRC),$(wildcard $(D)/*.c))

# create a %.o for every %.c file
OBJ = $(patsubst %.c,%.o,$(FILES))
# create a %.d for every %.c file
DEP = $(patsubst %.c,%.d,$(FILES))
# create a -l% for every library %
LNK = $(patsubst %,-l%,$(LIB))

all: $(BIN)

run: $(BIN)
	./$(BIN) $(ARG)

$(BIN): $(OBJ)
	$(CC) -o $@ $^ $(LNK)

%.o: %.c
	$(CC) $(FLAGS) -c -o $@ $<

clean:
	rm -rf $(BIN) $(OBJ) $(DEP)

-include $(DEP)

.PHONY: all clean
