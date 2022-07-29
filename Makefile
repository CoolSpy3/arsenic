OBJS := $(patsubst src/cpp/%.cpp,bin/%.o,$(wildcard src/cpp/*.cpp))
DEBUG_OBJS := $(patsubst src/cpp/%.cpp,bin/debug/%.o,$(wildcard src/cpp/*.cpp))

-include $(wildcard bin/*.d)
-include $(wildcard bin/debug/*.d)

bin/%.o: src/cpp/%.cpp
	mkdir -p $(@D)
	g++ -c -Wall -Werror -O2 --std=c++17 -Isrc/include -MMD -MF $@.d -MP -o $@ $<

bin/debug/%.o: src/cpp/%.cpp
	mkdir -p $(@D)
	g++ -c -g -Wall -Werror -O0 --std=c++17 -Isrc/include -MMD -MF $@.d -MP -o $@ $<

bin/arsenic.exe: $(OBJS)
	g++ -Wall -Werror -O2 --std=c++17 -mconsole -o bin/arsenic.exe $(OBJS)

bin/debug/arsenic.exe: $(DEBUG_OBJS)
	g++ -g -Wall -Werror -O0 --std=c++17 -mconsole -o bin/debug/arsenic.exe $(DEBUG_OBJS)

.PHONY: clean build rebuild debug

clean:
	rm -rf bin

build: bin/arsenic.exe

debug: bin/debug/arsenic.exe

rebuild: | clean build