DIR_Config   = ./lib/Config
DIR_EPD      = ./lib/LCD
DIR_GUI      = ./lib/GUI
DIR_Examples = ./examples
DIR_BIN      = ./bin

OBJ_C = $(wildcard ${DIR_EPD}/*.c ${DIR_Config}/*.c ${DIR_GUI}/*.c ${DIR_Examples}/*.c)
OBJ_O = $(patsubst %.c,${DIR_BIN}/%.o,$(notdir ${OBJ_C}))

TARGET = main

LIB = -llgpio -lm

CC = gcc
MSG = -g -O0 -Wall
CFLAGS += $(MSG)

${TARGET}:${OBJ_O}
        $(CC) $(CFLAGS) $(OBJ_O) -o $@ $(LIB)

${DIR_BIN}/%.o:$(DIR_Examples)/%.c
        $(CC) $(CFLAGS) -c  $< -o $@ -I $(DIR_Config) -I $(DIR_GUI) -I $(DIR_EPD)

${DIR_BIN}/%.o:$(DIR_EPD)/%.c
        $(CC) $(CFLAGS) -c  $< -o $@ -I $(DIR_Config)

${DIR_BIN}/%.o:$(DIR_GUI)/%.c
        $(CC) $(CFLAGS) -c  $< -o $@ -I $(DIR_Config)  -I $(DIR_EPD) -I $(DIR_Examples)

${DIR_BIN}/%.o:$(DIR_Config)/%.c
        $(CC) $(CFLAGS) -c  $< -o $@ $(LIB)

clean :
        rm $(DIR_BIN)/*.*
        rm $(TARGET)