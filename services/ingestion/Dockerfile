FROM python:3.12.13-alpine3.22@sha256:a190708a2dec1bd18b1decb539f8e8f5407abaa9bf39cacda583f7f8c11db322

ENV PYTHONDONTWRITEBYTECODE=1 \
    PYTHONUNBUFFERED=1 \
    SOURCETRACE_INGESTION_DB=/data/ingestion.sqlite3

RUN apk add --no-cache poppler-utils

WORKDIR /app
COPY pyproject.toml ./
COPY src ./src
RUN pip install --no-cache-dir .

RUN adduser -D -u 10001 worker \
    && mkdir /data \
    && chown worker:worker /data
USER worker

EXPOSE 8080
HEALTHCHECK --interval=10s --timeout=3s --retries=3 \
  CMD python -c "import urllib.request; urllib.request.urlopen('http://127.0.0.1:8080/healthz', timeout=2)"

CMD ["uvicorn", "sourcetrace_ingestion.main:app", "--host", "0.0.0.0", "--port", "8080"]
