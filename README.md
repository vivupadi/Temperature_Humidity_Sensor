# Temperature & Humidity (IoT) System

A continuous temperature and humidity sensor to monitor and observe the changes using Grafana Dashboard. 
Alerts can be added using Grafana alerting system + Telegram

## Tech Stack

#### Hardware

1. ESP32 Dev C1
2. DHT11 Temperature and Humidity Sensor
3. LED
4. 220 Ohm Resistor

#### Software

1. MQTT framework
2. HiveMQ
4. InfluxDB
5. Telegraf
6. Grafana

Published-subscriber approach to update the time-series data. It uses HiveMQ to subscribe to the data, which is then transferred to InfluxDB using Telegraf.
