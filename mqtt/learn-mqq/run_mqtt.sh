#/bin/sh
docker run -it -p 1883:1883 -v $(pwd)/mosquitto.conf:/mosquitto/config/mosquitto.conf --name mosquitto-broker eclipse-mosquitto
