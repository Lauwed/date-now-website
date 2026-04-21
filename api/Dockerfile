FROM alpine:3.23.3 AS base

RUN apk update && apk add --no-cache \
    libjwt2 \
    openssl \
    sqlite-libs \
    curl \
    imagemagick \
    libwebp

FROM base AS dev

RUN apk add --no-cache \
    alpine-sdk \
    libjwt2-dev \
    openssl-dev \
    sqlite-dev \
    curl-dev \
    imagemagick-dev \
    pkgconfig \
    bear \
    nodejs

WORKDIR /app

RUN mkdir -p bin obj/lib obj/endpoints obj/sql /var/www/uploads/media

COPY . .

RUN node doc/validate-json-schema.mjs doc/api.json

RUN bear -- make

EXPOSE 8000

CMD ["./bin/serv_api"]

FROM base AS run

RUN rm -rf /sbin/apk /etc/apk /lib/apk /usr/share/apk /var/lib/apk

WORKDIR /app

RUN mkdir -p /var/www/uploads/media

COPY --from=dev /app/bin /app/bin
COPY index.html .
COPY doc/ doc/

CMD ["/app/bin/serv_api"]

