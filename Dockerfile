FROM debian:stable
RUN apt-get update && \
    apt-get install -y python3 python3-pip && \
    pip3 install Adafruit_BBIO pytest
WORKDIR /app
COPY . /app
CMD ["pytest", "test/"]
