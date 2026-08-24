# Stage 1: Build C++20 Core
FROM gcc:13 AS builder

WORKDIR /app
COPY include/ ./include/
COPY src/ ./src/
COPY Makefile ./

RUN make all

# Stage 2: Runtime image with Python Telemetry Server
FROM python:3.11-slim

WORKDIR /app
COPY --from=builder /app/build/openpath_daemon /app/openpath_daemon
COPY server/ ./server/
COPY ml/ ./ml/
COPY tests/ ./tests/
COPY benchmarks/ ./benchmarks/

EXPOSE 8080

CMD ["python3", "server/app.py"]
