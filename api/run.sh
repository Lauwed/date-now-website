#!/bin/bash
export $(cat .env | xargs)
make

if [[ "$1" == "debug" ]]; then
	echo "Starting gdb" >&2
	gdb ./bin/serv_api
else
	./bin/serv_api
fi
