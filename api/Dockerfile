FROM ubuntu:24.04
RUN apt-get update && apt-get install -y \
    build-essential \
    libjwt-dev \
    libssl-dev \
    libcurl4-openssl-dev \
    sqlite3 \
    gdb \
    && rm -rf /var/lib/apt/lists/*

WORKDIR /app

RUN mkdir -p bin obj/lib obj/endpoints obj/sql

COPY . .

RUN make

EXPOSE 8000

CMD ["./bin/serv_api"]
