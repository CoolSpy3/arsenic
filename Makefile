OBJS := $(patsubst src/cpp/%.cpp,bin/%.o,$(wildcard src/cpp/*.cpp))

-include $(wildcard bin/*.d)

bin/%.o: src/cpp/%.cpp
	mkdir -p $(@D)
	g++ -c -Wall -Werror -O2 --std=c++17 -Isrc/include -MMD -MF $@.d -MP -o $@ $<

bin/arsenic.exe: $(OBJS)
	g++ -Wall -Werror -O2 --std=c++17 -Isrc/include -o bin/arsenic.exe $(OBJS)

.PHONY: clean build rebuild

clean:
	rm -rf bin

build: bin/arsenic.exe

rebuild: | clean build