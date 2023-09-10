# 1 "/tmp/tmpa0yvirz1"
#include <Arduino.h>
# 1 "/tmp/PicoPrometheus/basic/src/basic.ino"
#include <PicoPrometheus.h>

PicoPrometheus::Registry prometheus;

PicoPrometheus::Gauge gauge(prometheus, "foo", "Example gauge");
PicoPrometheus::Counter counter(prometheus, "bar", "Example counter");
PicoPrometheus::Histogram histogram(prometheus, "baz", "Example histogram");
void setup();
void loop();
#line 9 "/tmp/PicoPrometheus/basic/src/basic.ino"
void setup() {
    Serial.begin(115200);
    randomSeed(2137);
}

void loop() {

    counter.increment();
    gauge.set(random(1000));
    histogram.observe(random(12));


    Serial.println(prometheus);

    delay(1000);
}