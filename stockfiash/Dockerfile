FROM debian:bookworm-slim AS stockfish-build

ARG STOCKFISH_VERSION=sf_17.1
RUN apt-get update \
    && apt-get install -y --no-install-recommends ca-certificates curl g++ git make \
    && git clone --branch "${STOCKFISH_VERSION}" --depth 1 https://github.com/official-stockfish/Stockfish.git /src/stockfish \
    && make -C /src/stockfish/src -j1 build ARCH=x86-64 \
    && strip /src/stockfish/src/stockfish

FROM python:3.12-slim-bookworm

ENV PYTHONDONTWRITEBYTECODE=1 \
    PYTHONUNBUFFERED=1 \
    STOCKFISH_PATH=/usr/local/bin/stockfish

RUN useradd --create-home --uid 10001 openmove
COPY --from=stockfish-build /src/stockfish/src/stockfish /usr/local/bin/stockfish
COPY requirements.txt /tmp/requirements.txt
RUN pip install --no-cache-dir -r /tmp/requirements.txt

WORKDIR /service
COPY app ./app
USER openmove
EXPOSE 8080
CMD ["python", "-m", "app"]
