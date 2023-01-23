#include <esp-prometheus.h>

Prometheus prometheus;

PrometheusGauge gauge(prometheus, "foo", "Example gauge");
PrometheusCounter counter(prometheus, "bar", "Example counter");
PrometheusHistogram histogram(prometheus, "baz", "Example histogram");

void setup() {
    Serial.begin(115200);
    randomSeed(2137);
}

void loop() {
    // update metrics
    counter.increment();
    gauge.set(random(1000));
    histogram.observe(random(12));

    // print metrics to serial
    Serial.println(prometheus);

    delay(1000);
}
