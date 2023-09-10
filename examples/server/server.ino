#include <ESP8266WebServer.h>
#include <ESP8266mDNS.h>

#include <PicoPrometheus.h>

#if __has_include("config.h")
#include "config.h"
#endif

#ifndef WIFI_SSID
#define WIFI_SSID "WiFi SSID"
#endif

#ifndef WIFI_PASSWORD
#define WIFI_PASSWORD "password"
#endif

ESP8266WebServer server(80);

PicoPrometheus::Registry prometheus;

PicoPrometheus::Gauge gauge(prometheus, "foo", "Example gauge");
PicoPrometheus::Counter counter(prometheus, "bar", "Example counter");
PicoPrometheus::Histogram histogram(prometheus, "baz", "Example histogram");

void setup() {
    Serial.begin(115200);
    WiFi.mode(WIFI_STA);

    WiFi.begin(WIFI_SSID, WIFI_PASSWORD);

    Serial.println("Connecting to WiFi network...");
    while (WiFi.status() != WL_CONNECTED) {
        delay(1000);
        Serial.print(".");
    }
    Serial.println("connected");

    MDNS.begin("esp-prometheus");  // optional

    // register endpoint before calling server.begin()
    prometheus.register_metrics_endpoint(server);

    // by default /metrics will be used to serve metrics, change by using:
    // prometheus.register_metrics_endpoint(server, "/custom/metrics/endpoint")

    server.begin();

    randomSeed(2137);
}

void loop() {
    counter.increment();
    gauge.set(random(1000));
    histogram.observe(random(12));
    server.handleClient();
    MDNS.update();
}
