#include "prometheus.h"

void PrometheusDumpable::dump(Print & print) const {
    dump([&print](const char * str) { print.write(str); });
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

namespace {

void dump_labels(PrometheusWriter write, const PrometheusLabels & labels) {
    if (labels.empty()) {
        return;
    }

    write("{");
    bool first = true;
    for (const auto & lv : labels) {
        const auto & label = lv.first;
        const auto & value = lv.second;
        if (!first) {
            write(",");
        }
        write(label.c_str());
        write("=\"");
        write(value.c_str());  // TODO: escaping?
        write("\"");
        first = false;
    }
    write("}");
}

}

void PrometheusSimpleMetricValue::dump(PrometheusWriter write, const std::string & name,
                                       const PrometheusLabels & labels) const {
    write(name.c_str());
    dump_labels(write, labels);
    write(" ");
    write(std::to_string(value).c_str());
    write("\n");
}

void Prometheus::dump(PrometheusWriter write) const {
    for (PrometheusMetric * PrometheusMetric : metrics) {
        PrometheusMetric->dump(write);
    }
}
