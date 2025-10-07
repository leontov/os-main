# syntax=docker/dockerfile:1.5

FROM python:3.11-slim

ENV PYTHONUNBUFFERED=1 \
    PYTHONDONTWRITEBYTECODE=1

WORKDIR /app

COPY requirements.txt ./
RUN pip install --no-cache-dir -r requirements.txt

COPY core core
COPY scripts scripts

ENTRYPOINT ["python", "scripts/soak.py"]
CMD ["--hours", "4"]
