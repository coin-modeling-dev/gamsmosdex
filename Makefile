all : gams2mosdex

gams2mosdex : src/gams2mosdex.o gmomcc.o gevmcc.o dctmcc.o
	$(CXX) -o $@ $^ $(LDFLAGS)

clean:
	rm -f *.o gams2mosdex

%.c : gams/apifiles/C/api/%.c
	cp $< $@

IFLAGS = -Igams/apifiles/C/api
WFLAGS = -Wall -Wextra -Wno-unused-parameter
CFLAGS = $(IFLAGS) $(WFLAGS) -g -O0 -std=c99
CXXFLAGS = $(IFLAGS) $(WFLAGS) -g -O0 -std=c++11

LDFLAGS = -ldl
LDFLAGS += -Wl,-rpath,\$$ORIGIN -Wl,-rpath,$(realpath gams)
#LDFLAGS += -Lhighs/lib -lhighs -Wl,-rpath,$(realpath highs/lib)
