VERSION    =  2.00
CC       =  gcc
DEBUG      =  -DUSE_DEBUG
CFLAGS     =  -Wall
SOURCES	   =  sendat.c
INCLUDES   =  -I
LIB_NAMES  =  
LIB_PATH   =  
OBJ        =  $(SOURCES:.c=.o)
TARGET     =  sendat

#links
$(TARGET):$(OBJ)
	$(CC) $(SOURCES) -o $(TARGET) $(LIB_NAMES)
	
#compile
%.o: %.c
	$(CC) $(INCLUDES) $(DEBUG) -c $(CFLAGS) $< -o $@

.PHONY:clean
clean:
	@echo "Remove linked and compiled files......"
	rm -rf $(OBJ) $(TARGET) output 


