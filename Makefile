CXXFLAGS = -std=c++17 -O2 -Wall -Wextra
LDLIBS = -lncursesw -lmenuw
OBJS = main.o ncurses-utils.o ncurses-widget.o qa-file.o qa-screens.o
EXE = main

$(EXE): $(OBJS)
	g++ -o $@ $^ $(LDLIBS)
$(OBJS): %.o: %.cpp

clean:
	rm -f $(OBJS) $(EXE)
