FROM alpine:3.10 as builder
RUN apk add --no-cache boost-dev boost-static g++ make cmake
COPY . /src
WORKDIR /src
RUN make relcmake && make


FROM alpine:3.10
RUN mkdir app
COPY --from=builder /src/build/zks /app/zks
RUN apk add --no-cache libstdc++
WORKDIR /data
VOLUME [ '/data' ]
ENTRYPOINT ["/app/zks"]
