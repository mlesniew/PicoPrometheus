# esp-prometheus

A C++ library for ESP8266 that allows easy building and exporting of Prometheus metrics.

## Features

* Simple API for creating and exporting Prometheus metrics
* Support for common metric types: counter, gauge and histogram
* Handling of metric labels
* Built-in support for ESP8266WebServer for easy integration with web applications.
* Lightweight and efficient design for use with resource-constrained devices.


## Installation

To use the library, you will need to have the ESP8266 board and toolchain set up. You can find instructions on how to do this on the Arduino website.

Once you have the ESP8266 board and toolchain set up, you can install the library by adding it to your Arduino libraries folder.


## Usage

Here is an example of how to use the library to create and export a simple counter metric:

```
#include <Arduino.h>
#include <ESP8266WebServer.h>
#include <prometheus.h>

#define WIFI_SSID "WiFi"
#define WIFI_PASSWORD "password"

ESP8266WebServer server(80);

Prometheus prometheus;

PrometheusCounter counter(prometheus, "foo", "Example counter");

void setup() {
    Serial.begin(115200);
    WiFi.mode(WIFI_STA);
    WiFi.begin(WIFI_SSID, WIFI_PASSWORD);

    while (WiFi.status() != WL_CONNECTED) {
        delay(100);
    }

    server.on("/foo", [] {
            counter[{{"bar": "baz"}, {"spam": "eggs"}].increment();
        });

    prometheus.register_metrics_endpoint(server);

    server.begin();
}

void loop() {
    server.handleClient();
}
```

In this example, a counter metric named `counter` is created and incremented every time the `/foo` endpoint is hit.  The `register_metrics_endpoint` call automatically creates a `/metrics` endpoint in the server.

For more detailed usage and examples, please see the example sketches.


## Contribute

Please feel free to contribute by opening issues and pull requests in the GitHub repository.


## License

The Prometheus C++ Library for ESP8266 is open-source software licensed under the GNU GPLv3 license.
