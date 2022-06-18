FLAGS_IGNORE_WARNINGS = -Wno-unused-command-line-argument -Wno-deprecated-declarations
FLAGS_COMMON = -framework IOKit -framework CoreFoundation
FLAGS = ${FLAGS_COMMON} ${FLAGS_IGNORE_WARNINGS} -std=c++2a

all: list_keyboards grab_keyboard
	echo "Done\n\n"

grab_keyboard: grab_keyboard.o keyio_mac.o
	g++ grab_keyboard.o keyio_mac.o -o grab_keyboard ${FLAGS}

grab_keyboard.o: grab_keyboard.cpp
	g++ -c grab_keyboard.cpp ${FLAGS}

keyio_mac.o: keyio_mac.cpp
	g++ -c keyio_mac.cpp ${FLAGS}

list_keyboards: list_keyboards.cpp
	g++ $< -o list_keyboards ${FLAGS}

clean:
	rm -f *.o grab_keyboard list_keyboards
