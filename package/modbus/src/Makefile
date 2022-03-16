VERSION    =  2.00
CC         =  gcc
DEBUG      =  -DUSE_DEBUG
CFLAGS     =  -Wall
SOURCES	   =  cfg.c conn.c crc16.c log.c main.c modbus.c queue.c sig.c sock.c state.c tty.c
INCLUDES   =  
LIB_NAMES  =  -lpthread -ldl
LIB_PATH   =  
OBJ        =  $(SOURCES:.c=.o)
TARGET     =  modbus

#links
$(TARGET):$(OBJ)
	@mkdir -p output
	$(CC) $(SOURCES) -o $(TARGET) $(LIB_NAMES)
	
#compile
%.o: %.c
	$(CC) $(INCLUDES) $(DEBUG) -c $(CFLAGS) $< -o $@

.PHONY:clean
clean:
	@echo "Remove linked and compiled files......"
	rm -rf $(OBJ) $(TARGET) output 


