CC := gcc
CFLAGS := -Wall -Wextra -g -std=gnu11 -O3 -march=native -DDEBUG #-fsanitize=address

YACC := bison
YFLAGS := #-t --report=all --language=c

TARGET_EXEC := jparse

BUILD_DIR := ./build
SRC_DIR := ./src

SRCS := jparse.c jsonobject/jsonobject.c
OBJS := build/jparse.o build/jsonobject.o

$(BUILD_DIR)/$(TARGET_EXEC): $(OBJS)
	$(CC) build/jparse.o build/jsonobject.o -o $@ $(CFLAGS) -flto

# How to make .o file
$(BUILD_DIR)/%.o: $(SRC_DIR)/%.c
	$(CC) $(CFLAGS) -c $^ -o $@

$(BUILD_DIR)/%.o: $(SRC_DIR)/jsonobject/jsonobject.c
	$(CC) $(CFLAGS) -c $^ -o build/jsonobject.o

%.c: %.y
	$(YACC) $(YFLAGS) -o $^.re $^
	re2c -o $@ $^.re --utf-8

.PHONY: clean run
.PRECIOUS: src/jparse.c src/jparse.re

run: $(BUILD_DIR)/$(TARGET_EXEC)
	@$^ data/test.json

clean:
	rm -rf $(BUILD_DIR)/*.o
	rm -rf $(BUILD_DIR)/$(TARGET_EXEC)
	rm -rf $(SRC_DIR)/jparse.c
	rm -rf $(SRC_DIR)/jparse.y.re

