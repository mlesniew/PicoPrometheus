#include <limits>

#include "prometheus.h"

namespace {

size_t print_labels(Print & print, const PrometheusLabels & global_labels,
        const PrometheusLabels & labels, const double le = std::numeric_limits<double>::quiet_NaN()) {

    if (global_labels.empty() && labels.empty() && std::isnan(le)) {
        return 0;
    }

    auto print_label = [&print](const std::string & label, const std::string & value, bool &first) {
        size_t ret = 0;
        if (!first) {
            ret += print.print(',');
        }
        ret += print.print(label.c_str());
        ret += print.print(F("=\""));
        ret += print.print(value.c_str());  // TODO: escaping?
        ret += print.print('"');
        first = false;
        return ret;
    };

    size_t ret = 0;
    bool first = true;
    ret += print.print('{');

    for (const auto & lv : global_labels) {
        ret += print_label(lv.first, lv.second, first);
    }

    for (const auto & lv : labels) {
        ret += print_label(lv.first, lv.second, first);
    }

    if (!std::isnan(le)) {
        ret += print_label("le", std::isinf(le) ? "+Inf" : std::to_string(le), first);
    }

    ret += print.print('}');

    return ret;
}

}

PrometheusMetric::PrometheusMetric(Prometheus & prometheus, const std::string & name, const std::string & help)
    : name(name), help(help), prometheus(prometheus) {
    prometheus.metrics.insert(this);
}

PrometheusMetric::~PrometheusMetric() {
    prometheus.metrics.erase(this);
}

size_t PrometheusMetric::printTo(Print & print) const {
    size_t ret = 0;

    if (metrics.empty()) {
        return ret;
    }

    // header
    auto print_header_line = [this, &print](const __FlashStringHelper * prefix, const char * value) {
        size_t ret = 0;
        ret += print.print(F("# "));
        ret += print.print(prefix);
        ret += print.print(F(" "));
        ret += print.print(name.c_str());
        ret += print.print(F(" "));
        ret += print.print(value);
        ret += print.print(F("\n"));
        return ret;
    };

    ret += print_header_line(F("HELP"), help.c_str());
    ret += print_header_line(F("TYPE"), get_prometheus_type_name());

    // metrics
    for (const auto & kv : metrics) {
        const auto & labels = kv.first;
        const auto & metric_ptr = kv.second;
        ret += metric_ptr->printTo(print, name, prometheus.labels, labels);
    }

    return ret;
}

PrometheusMetricValue & PrometheusMetric::get(const PrometheusLabels & labels) {
    auto & ptr = metrics[labels];
    if (!ptr) {
        ptr = construct_value();
    }
    return *ptr;
}

size_t PrometheusSimpleMetricValue::printTo(Print & print, const std::string & name,
        const PrometheusLabels & global_labels, const PrometheusLabels & labels) const {
    size_t ret = 0;
    ret += print.print(name.c_str());
    ret += print_labels(print, global_labels, labels);
    ret += print.print(' ');
    ret += print.print(std::to_string(value).c_str());
    ret += print.print("\n");
    return ret;
}

const std::vector<double> PrometheusHistogramMetricValue::defalut_buckets = {.005, .01, .025, .05, .075, .1, .25, .5, .75, 1.0, 2.5, 5.0, 7.5, 10.0};

PrometheusHistogramMetricValue::PrometheusHistogramMetricValue(const std::vector<double> & buckets)
    : count(0) {
    for (const auto & e : buckets) {
        this->buckets[e] = 0;
    }
}

void PrometheusHistogramMetricValue::observe(double value) {
    for (auto & kv : buckets) {
        const auto & threshold = kv.first;
        if (value <= threshold) {
            ++kv.second;
        }
    }
    sum += value;
    ++count;
}

size_t PrometheusHistogramMetricValue::printTo(Print & print, const std::string & name,
        const PrometheusLabels & global_labels, const PrometheusLabels & labels) const {

    auto print_line = [this, &print, &name, &labels, &global_labels](const char * suffix, double value,
    double le = std::numeric_limits<double>::quiet_NaN()) {
        size_t ret = 0;
        ret += print.print(name.c_str());
        ret += print.print(suffix);
        ret += print_labels(print, global_labels, labels, le);
        ret += print.print(" ");
        ret += print.print(std::to_string(value).c_str());
        ret += print.print("\n");
        return ret;
    };

    size_t ret = 0;

    ret += print_line("_count", count);
    ret += print_line("_bucket", count, std::numeric_limits<double>::infinity());
    ret += print_line("_sum", sum);

    for (const auto & kv : buckets) {
        ret += print_line("_bucket", kv.second, kv.first);
    }

    return ret;
}

size_t Prometheus::printTo(Print & print) const {
    size_t ret = 0;
    for (PrometheusMetric * PrometheusMetric : metrics) {
        ret += PrometheusMetric->printTo(print);
    }
    return ret;
}

#ifdef ESP8266
void Prometheus::register_metrics_endpoint(ESP8266WebServer & server, const Uri & uri) {

    class ServerReplyPrinter: public Print {
        public:
            ServerReplyPrinter(ESP8266WebServer & server) : server(server) {}

            size_t write(uint8_t c) override {
                server.sendContent(reinterpret_cast<const char *>(&c), 1);
                return 1;
            }

            size_t write(const uint8_t * buffer, size_t size) override {
                server.sendContent(reinterpret_cast<const char *>(buffer), size);
                return size;
            }

            ESP8266WebServer & server;
    };

    server.on(uri, HTTP_GET, [this, &server] {
        server.setContentLength(CONTENT_LENGTH_UNKNOWN);
        server.send(200, F("plain/text"), F(""));
        ServerReplyPrinter srp(server);
        printTo(srp);
    });
}
#endif

