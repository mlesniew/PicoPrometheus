#include <prometheus.h>

// global labels can be defined during construction
Prometheus prometheus({
    { "module", "esp8266" },
    { "build_date", __DATE__ },
});

PrometheusGauge gauge(prometheus, "foo", "Example gauge");
PrometheusCounter counter(prometheus, "bar", "Example counter");
PrometheusHistogram histogram(prometheus, "baz", "Example histogram");

void setup() {
    Serial.begin(115200);
    randomSeed(2137);

    // labels can be added, updated and removed dynamically at runtime too (can be done in the loop() too)
    prometheus.labels["extra_label"] = "added";
    prometheus.labels["extra_label"] = "updated";
    prometheus.labels.erase("extra_label");  // removed
}

void loop() {
    // update metrics without labels
    counter.increment();
    gauge.set(random(1000));
    histogram.observe(random(12));

    // update labelled metrics
    counter[ {{"color", "blue"}, {"mood", "sad"}}].increment(random(5));
    gauge[ {{"temperature", "bedroom"}}].set(random(30));
    histogram[ {{"dinosaur", "triceratops"}}].observe(random(50));

    // print metrics to serial
    Serial.println(prometheus);

    delay(1000);
}
