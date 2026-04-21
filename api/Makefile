CC ?= cc
MAGICK_PKG    := $(shell pkg-config --list-all 2>/dev/null | grep -i '^MagickWand' | head -1 | cut -d' ' -f1)
MAGICK_CFLAGS := $(shell pkg-config --cflags $(MAGICK_PKG))
MAGICK_LIBS   := $(shell pkg-config --libs   $(MAGICK_PKG))
CFLAGS := -I ./include/ -g $(MAGICK_CFLAGS)
LIBS := -ljwt -lssl -lcrypto -lsqlite3 -lcurl -lcjson $(MAGICK_LIBS)

includes := ./include
src := ./src

tables := user tag sponsor issue issue_tag issue_author issue_sponsor view auth media

libs_o := obj/lib/mongoose.o obj/lib/validatejson.o obj/lib/email.o obj/lib/totp.o
helpers_o := obj/utils.o obj/lib/rate_limiter.o
endpoints_o := ${foreach t,$(tables),obj/endpoints/$(t).o}
sql_o := ${foreach t,$(tables),obj/sql/$(t).o}

helpers_h := $(includes)/utils.h $(includes)/structs.h $(includes)/enums.h

serv_api : obj/main.o $(libs_o) $(helpers_o) $(endpoints_o) $(sql_o)
	$(CC) -o bin/serv_api obj/main.o $(libs_o) $(endpoints_o) $(sql_o) $(helpers_o) $(LIBS) -g -lm -Wall -O2


obj/main.o : ./src/main.c $(libs_o) $(endpoints_o) $(helpers_o)
	$(CC) -c $< $(CFLAGS) -o $@

$(endpoints_o) : obj/endpoints/%.o : src/endpoints/%.c $(libs_o) $(helpers_o) $(helpers_h)
	$(CC) -c $< $(CFLAGS) -o $@

$(sql_o) : obj/sql/%.o : src/sql/%.c $(libs_o) $(helpers_o) $(helpers_h)
	$(CC) -c $< $(CFLAGS) -o $@

$(helpers_o) : obj/%.o : src/%.c $(libs_o)
	$(CC) -c $< $(CFLAGS) -o $@

$(libs_o) : obj/lib/%.o : src/lib/%.c
	$(CC) -c $< $(CFLAGS) -o $@

clean :
	rm -f bin/serv_api $(wildcard ./obj/*.o) $(wildcard ./obj/lib/*.o) $(wildcard ./obj/endpoints/*.o) $(wildcard ./obj/sql/*.o)
