CFLAGS += -Wall -Wextra -pedantic -Werror -std=c99

ifdef DEBUG
	CFLAGS += -g -DWS_PARSER_DUMP_STATE
else
	CFLAGS += -O3
endif

T = ws/codec

.PHONY: default test clean

default: ws_parser.o $(T).so

$(T).so: ws.c ws_parser.c
	mkdir -p ws
	$(CC) -shared -o $@ $(CFLAGS) -fPIC $^ \
		$(shell pkg-config --cflags luajit) $(shell pkg-config --libs luajit)

test: test/parse
	ruby test/driver.rb

clean:
	rm -rf *.o *.so ws test/parse test/parse.o

%.o: %.c ws_parser.h
	$(CC) -o $@ $(CFLAGS) -c $<

test/parse: test/parse.o ws_parser.o
test/parse.o: CFLAGS+=-iquote .
