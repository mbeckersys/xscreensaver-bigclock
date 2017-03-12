CC=g++
CXXFLAGS=`sdl2-config --cflags`
CXXFLAGS+=-Wall -g
LDFLAGS=`sdl2-config --libs`
LDFLAGS+=-lX11 -lSDL2_ttf -lSDL2_gfx
EXE=bigclock
SRC=main.cpp

XML=$(EXE).xml
OBJS=$(SRC:.cpp=.o)

all: $(EXE)

%.o: %.cpp
	$(CC) $(CXXFLAGS) -c $< -o $@

$(EXE): $(OBJS)
	$(CC) $(LDFLAGS) $< -o $@
install:
	install -o root $(EXE) /usr/lib/xscreensaver/
	install -o root $(XML) /usr/share/xscreensaver/config/		
clean:
	rm -rf $(OBJS) $(EXE)
