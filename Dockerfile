FROM debian:bookworm AS builder

LABEL maintainer="movoid"

VOLUME test_vol

RUN apt update && apt install -y build-essential git cmake

RUN mkdir /app
WORKDIR /app
COPY . .

RUN cmake -DCMAKE_BUILD_TYPE=Release -DCMAKE_INSTALL_PREFIX=/app/install -S /app -B /app/build
RUN cmake --build /app/build
RUN cmake --install /app/build


FROM debian:bookworm-slim

RUN apt-get update && apt-get install -y libstdc++6

WORKDIR /app
COPY --from=builder /app/install /usr

RUN useradd -m runner
USER runner

CMD ["/bin/bash"]
