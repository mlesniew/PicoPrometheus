#include <limits>

#include "PicoPrometheus.h"

namespace {

using namespace PicoPrometheus;

String double_to_str(double value) {
    if (std::isnan(value)) {
        return "NaN";
    } else if (!std::isfinite(value)) {
        if (value > 0) {
            return "+Inf";
        } else {
            return "-Inf";
        }
    }

    constexpr size_t buffer_size = std::numeric_limits<double>::max_digits10 + 1;
    char buffer[buffer_size];
    snprintf(buffer, buffer_size, "%g", value);
    return buffer;
}

size_t print_labels(Print & print, const Labels & global_labels,
                    const Labels & labels, const double le = std::numeric_limits<double>::quiet_NaN()) {

    if (global_labels.empty() && labels.empty() && std::isnan(le)) {
        return 0;
    }

    auto print_label = [&print](const String & label, const String & value, bool & first) {
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
        ret += print_label("le", double_to_str(le), first);
    }

    ret += print.print('}');

    return ret;
}

}


namespace PicoPrometheus {

bool Labels::is_subset_of(const Labels & other) const {
    for (auto & kv : *this) {
        const auto it = other.find(kv.first);
        if (it == other.end()) {
            return false;
        }
        if (it->second != kv.second) {
            return false;
        }
    }
    return true;
}

Metric::Metric(Registry & registry, const String & name, const String & help)
    : name(name), help(help), registry(registry) {
    registry.metrics.insert(this);
}

Metric::~Metric() {
    registry.metrics.erase(this);
}

void Metric::remove(const Labels & labels, bool exact_match) {
    if (exact_match) {
        metrics.erase(labels);
    } else {
        auto it = metrics.begin();

        while (it != metrics.end()) {
            if (labels.is_subset_of(it->first)) {
                it = metrics.erase(it);
            } else {
                ++it;
            }
        }

    }
}

void Metric::clear() {
    metrics.clear();
}

size_t Metric::printTo(Print & print) const {
    size_t ret = 0;

    if (metrics.empty()) {
        return ret;
    }

    // header
    auto print_header_line = [this, &print](const char * prefix, const char * value) {
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

    ret += print_header_line("HELP", help.c_str());
    ret += print_header_line("TYPE", get_prometheus_type_name());

    // metrics
    for (const auto & kv : metrics) {
        const auto & labels = kv.first;
        const auto & metric_ptr = kv.second;
        ret += metric_ptr->printTo(print, name, registry.labels, labels);
    }

    return ret;
}

MetricValue & Metric::get(const Labels & labels) {
    auto & ptr = metrics[labels];
    if (!ptr) {
        ptr = construct_value();
    }
    return *ptr;
}

size_t SimpleMetricValue::printTo(Print & print, const String & name,
                                  const Labels & global_labels, const Labels & labels) const {
    size_t ret = 0;
    ret += print.print(name.c_str());
    ret += print_labels(print, global_labels, labels);
    ret += print.print(' ');
    ret += print.print(double_to_str(get_value()).c_str());
    ret += print.print("\n");
    return ret;
}

const std::vector<double> HistogramMetricValue::defalut_buckets = {.005, .01, .025, .05, .075, .1, .25, .5, .75, 1.0, 2.5, 5.0, 7.5, 10.0};

HistogramMetricValue::HistogramMetricValue(const std::vector<double> & buckets)
    : count(0), sum(0) {
    for (const auto & e : buckets) {
        this->buckets[e] = 0;
    }
}

void HistogramMetricValue::observe(double value) {
    for (auto & kv : buckets) {
        const auto & threshold = kv.first;
        if (value <= threshold) {
            ++kv.second;
        }
    }
    sum += value;
    ++count;
}

size_t HistogramMetricValue::printTo(Print & print, const String & name,
                                     const Labels & global_labels, const Labels & labels) const {

    auto print_line = [this, &print, &name, &labels, &global_labels](const char * suffix, double value,
    double le = std::numeric_limits<double>::quiet_NaN()) {
        size_t ret = 0;
        ret += print.print(name.c_str());
        ret += print.print(suffix);
        ret += print_labels(print, global_labels, labels, le);
        ret += print.print(" ");
        ret += print.print(double_to_str(value).c_str());
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

size_t Registry::printTo(Print & print) const {
    size_t ret = 0;
    for (Metric * Metric : metrics) {
        ret += Metric->printTo(print);
    }
    return ret;
}

}
