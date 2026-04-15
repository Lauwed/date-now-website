#!/bin/bash
export $(cat .env | xargs)
gdb ./bin/serv_api
