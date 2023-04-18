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

Prometheus prometheus({
    {"board", "esp8266"},
    {"example", "advanced.ino"}
});

PrometheusGauge gauge(prometheus, "example_intensity", "Color intensity");
PrometheusCounter counter(prometheus, "example_updates", "Color intensity updates");
PrometheusHistogram histogram(prometheus, "example_intensity_distribution", "Example histogram",
{64, 128, 192, std::numeric_limits<double>::infinity()});

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

    MDNS.begin("esp-prometheus");
    prometheus.labels["hostname"] = "esp-hostname";

    // register endpoint before calling server.begin()
    prometheus.register_metrics_endpoint(server);

    server.begin();

    randomSeed(2137);
}

void loop() {

    const char * color;

    switch (random(3)) {
        case 0: color = "red"; break;
        case 1: color = "green"; break;
        default: color = "blue"; break;
    };

    int value = random(256);

    gauge[ {{"color", color}}].set(value);

    counter.increment();
    counter[ {{"color", color}}].increment();

    histogram.observe(value);
    histogram[ {{"color", color}, {"favourite", random(2) == 0 ? "true" : "false"}}].observe(value);

    server.handleClient();
    MDNS.update();
}
