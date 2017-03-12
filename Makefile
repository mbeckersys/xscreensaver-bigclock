CC=g++
CXXFLAGS=`sdl-config --cflags`
CXXFLAGS+=-Wall
LDFLAGS=`sdl-config --libs`
LDFLAGS+=-lX11 -lSDL_ttf -lSDL_gfx
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
