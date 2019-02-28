all : gams2mosdex mosdex2gams

gams2mosdex : src/gams2mosdex.o src/loadgms.o gmomcc.o gevmcc.o dctmcc.o
	$(CXX) -o $@ $^ $(LDFLAGS)

mosdex2gams : src/mosdex2gams.o
	$(CXX) -o $@ $^ $(LDFLAGS)

clean:
	rm -f *.o src/*.o gams2mosdex mosdex2gams

%.c : gams/apifiles/C/api/%.c
	cp $< $@

IFLAGS = -Igams/apifiles/C/api -DGAMSDIR=\"$(realpath gams)\"
WFLAGS = -Wall -Wextra -Wno-unused-parameter
CFLAGS = $(IFLAGS) $(WFLAGS) -g -O0 -std=c99
CXXFLAGS = $(IFLAGS) $(WFLAGS) -g -O0 -std=c++11

LDFLAGS = -ldl
LDFLAGS += -Wl,-rpath,\$$ORIGIN -Wl,-rpath,$(realpath gams)
#LDFLAGS += -Lhighs/lib -lhighs -Wl,-rpath,$(realpath highs/lib)
