FROM ubuntu:26.04 AS builder

RUN apt-get update && apt-get install -y --no-install-recommends \
    g++ libpq-dev nlohmann-json3-dev ca-certificates && rm -rf /var/lib/apt/lists/*

WORKDIR /app
COPY main.cpp httplib.h ./
COPY vendor ./vendor
COPY public ./public
COPY schema.sql ./

RUN g++ -std=c++17 -O2 -I. -Ivendor -Ivendor/nlohmann main.cpp -lpq -pthread -o musicapp

FROM ubuntu:26.04
RUN apt-get update && apt-get install -y --no-install-recommends libpq5 ca-certificates \
    && rm -rf /var/lib/apt/lists/*

WORKDIR /app
COPY --from=builder /app/musicapp /app/musicapp
COPY --from=builder /app/public /app/public
COPY --from=builder /app/schema.sql /app/schema.sql

ENV PORT=8080
EXPOSE 8080

CMD ["/app/musicapp"]
