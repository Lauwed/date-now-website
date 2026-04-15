FROM alpine:3.23.3 AS base

RUN apk update && apk add --no-cache \
    libjwt2 \
    openssl \
    sqlite \
    curl

FROM base AS dev

RUN apk add --no-cache \
    alpine-sdk \
    libjwt2-dev \
    openssl-dev \
    sqlite-dev \
    curl-dev

WORKDIR /app

RUN mkdir -p bin obj/lib obj/endpoints obj/sql

COPY . .

RUN make

EXPOSE 8000

CMD ["./bin/serv_api"]

FROM base AS run

RUN rm -rf /sbin/apk /etc/apk /lib/apk /usr/share/apk /var/lib/apk

COPY --from=dev /app/bin /app/bin

CMD ["/app/bin/serv_api"]

