CC=gcc
CFLAGS=-Wall -Wextra
TARGET=lab2

build: 
	$(CC) $(CFLAGS) -o $(TARGET)_add $(TARGET)_add.c -lpthread -lrt
	$(CC) $(CFLAGS) -o $(TARGET)_list $(TARGET)_list.c SortedList.c -lpthread -lrt

tests: build
	rm -f $(TARGET)_add.csv
	rm -f $(TARGET)_list.csv
	chmod +x gen_data.sh
	-./gen_data.sh 2> /dev/null

graphs: tests
	./$(TARGET)_add.gp 2> /dev/null
	./$(TARGET)_list.gp 2> /dev/null

dist: graphs
	tar -czvf lab2a-505111868.tar.gz lab2_add.c SortedList.h SortedList.c lab2_list.c Makefile README lab2_add.csv lab2_list.csv lab2_add-1.png lab2_add-2.png lab2_add-3.png lab2_add-4.png lab2_add-5.png lab2_list-1.png lab2_list-2.png lab2_list-3.png lab2_list-4.png gen_data.sh lab2_add.gp lab2_list.gp

clean:
	rm -f *.o $(TARGET)_add $(TARGET)_list $(TARGET)a-505111868.tar.gz
