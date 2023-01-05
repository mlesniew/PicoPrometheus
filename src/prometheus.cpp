#include <limits>

#include "prometheus.h"

namespace {

void dump_labels(PrometheusWriter write, const PrometheusLabels & labels,
                 const double le = std::numeric_limits<double>::quiet_NaN()) {
    if (labels.empty() && std::isnan(le)) {
        return;
    }

    bool first = true;

    auto write_label = [&write](const std::string & label, const std::string & value, bool first) {
        if (!first) {
            write(",");
        }
        write(label.c_str());
        write("=\"");
        write(value.c_str());  // TODO: escaping?
        write("\"");
    };

    write("{");
    for (const auto & lv : labels) {
        const auto & label = lv.first;
        const auto & value = lv.second;
        write_label(label, value, first);
        first = false;
    }

    if (!std::isnan(le)) {
        write_label("le", std::isinf(le) ? "+Inf" : std::to_string(le), first);
    }

    write("}");
}

}
}

#ifdef ESP8266
void PrometheusDumpable::register_metrics_endpoint(ESP8266WebServer & server, const Uri & uri) {
    server.on(uri, HTTP_GET, [this, &server] {
        server.setContentLength(CONTENT_LENGTH_UNKNOWN);
        server.send(200, F("text/html"), F(""));
        dump([&server](const char * buf) { server.sendContent(buf); });
    });
}
#endif

PrometheusMetric::PrometheusMetric(Prometheus & prometheus, const std::string & name, const std::string & help)
    : name(name), help(help), prometheus(prometheus) {
    prometheus.metrics.insert(this);
}

PrometheusMetric::~PrometheusMetric() {
    prometheus.metrics.erase(this);
}

void PrometheusMetric::dump(PrometheusWriter write) const {
    if (metrics.empty()) {
        return;
    }

    // header
    auto write_header_line = [this, write](const char * prefix, const char * value) {
        write("# ");
        write(prefix);
        write(" ");
        write(name.c_str());
        write(" ");
        write(value);
        write("\n");
    };

    write_header_line("HELP", help.c_str());
    write_header_line("TYPE", get_prometheus_type_name());

    // metrics
    for (const auto & kv : metrics) {
        const auto & labels = kv.first;
        const auto & metric_ptr = kv.second;
        metric_ptr->dump(write, name, labels);
    }
}

PrometheusMetricValue & PrometheusMetric::get(const PrometheusLabels & labels) {
    auto & ptr = metrics[labels];
    if (!ptr) {
        ptr = construct_value();
    }
    return *ptr;
}

void PrometheusSimpleMetricValue::dump(PrometheusWriter write, const std::string & name,
                                       const PrometheusLabels & labels) const {
    write(name.c_str());
    dump_labels(write, labels);
    write(" ");
    write(std::to_string(value).c_str());
    write("\n");
}

const std::vector<double> PrometheusHistogramMetricValue::defalut_buckets = {.005, .01, .025, .05, .075, .1, .25, .5, .75, 1.0, 2.5, 5.0, 7.5, 10.0, std::numeric_limits<double>::infinity()};

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
    ++count;
}

void PrometheusHistogramMetricValue::dump(PrometheusWriter write, const std::string & name,
        const PrometheusLabels & labels) const {
    auto write_line = [this, &write, &name, &labels](const char * suffix, unsigned long value,
    double le = std::numeric_limits<double>::quiet_NaN()) {
        write(name.c_str());
        write(suffix);
        dump_labels(write, labels, le);
        write(" ");
        write(std::to_string(value).c_str());
        write("\n");
    };

    write_line("_count", count);
    for (const auto & kv : buckets) {
        write_line("_bucket", kv.second, kv.first);
    }
}

void Prometheus::dump(PrometheusWriter write) const {
    for (PrometheusMetric * PrometheusMetric : metrics) {
        PrometheusMetric->dump(write);
    }
}
