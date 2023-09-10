#pragma once

#include <functional>
#include <map>
#include <memory>
#include <set>
#include <vector>

#include <Arduino.h>


namespace PicoPrometheus {

class Labels: public std::map<String, String> {
    public:
        using std::map<String, String>::map;
        bool is_subset_of(const Labels & other) const;
};

class Registry;
class Metric;

class MetricValue {
    public:
        MetricValue() = default;
        MetricValue(const MetricValue &) = delete;
        MetricValue & operator=(const MetricValue &) = delete;

    protected:
        virtual size_t printTo(Print & print, const String & name, const Labels & global_labels, const Labels & labels) const = 0;

        friend class Metric;
};

class Metric: public Printable {
    public:
        Metric(Registry & registry, const String & name, const String & help);
        virtual ~Metric();

        Metric(const Metric &) = delete;
        Metric & operator=(const Metric &) = delete;

        size_t printTo(Print & print) const final;

        void remove(const Labels & labels, bool exact_match = true);
        void clear();

        const String name;
        const String help;

    protected:
        MetricValue & get(const Labels & labels);

        virtual std::unique_ptr<MetricValue> construct_value() const = 0;
        virtual const char * get_prometheus_type_name() const = 0;

    private:
        std::map<Labels, std::unique_ptr<MetricValue>> metrics;
        Registry & registry;
};


template <typename T>
class TypedMetric: public Metric {
    public:
        using Metric::Metric;

        T & operator[](const Labels & labels) {
            return static_cast<T &>(get(labels));
        }

        T & get_default_metric() { return (*this)[ {}]; }

    protected:
        std::unique_ptr<MetricValue> construct_value() const override {
            return std::unique_ptr<MetricValue>(new T());
        }
};


class SimpleMetricValue: public MetricValue {
    public:
        SimpleMetricValue(): value(0) {}

    protected:
        size_t printTo(Print & print, const String & name, const Labels & global_labels, const Labels & labels) const override;

        double value;
};

class GaugeValue: public SimpleMetricValue {
    public:
        using SimpleMetricValue::SimpleMetricValue;
        void set(double value) { this->value = value; }
};

class CounterValue: public SimpleMetricValue {
    public:
        using SimpleMetricValue::SimpleMetricValue;
        void increment(double value = 1.0) { if (value > 0.0) this->value += value; }
};

class Gauge: public TypedMetric<GaugeValue> {
    public:
        using TypedMetric<GaugeValue>::TypedMetric;
        void set(double value) { get_default_metric().set(value); }

    protected:
        const char * get_prometheus_type_name() const override { return "gauge"; }
};

class Counter: public TypedMetric<CounterValue> {
    public:
        using TypedMetric<CounterValue>::TypedMetric;
        void increment(double value = 1.0) { get_default_metric().increment(value); }

    protected:
        const char * get_prometheus_type_name() const override { return "counter"; }
};

class HistogramMetricValue: public MetricValue {
    public:
        static const std::vector<double> defalut_buckets;

        HistogramMetricValue(const std::vector<double> & buckets = HistogramMetricValue::defalut_buckets);

        void observe(double value);

    protected:
        size_t printTo(Print & print, const String & name, const Labels & global_labels, const Labels & labels) const override;

        unsigned long count;
        std::map<double, unsigned long> buckets;
        double sum;
};

class Histogram: public TypedMetric<HistogramMetricValue> {
    public:
        Histogram(Registry & registry, const String name, const String help,
                            const std::vector<double> & buckets = HistogramMetricValue::defalut_buckets)
            : TypedMetric<HistogramMetricValue>(registry, name, help), buckets(buckets) {}

        void observe(double value) { get_default_metric().observe(value); }

        const std::vector<double> buckets;

    protected:
        const char * get_prometheus_type_name() const override { return "histogram"; }

        std::unique_ptr<MetricValue> construct_value() const override {
            return std::unique_ptr<MetricValue>(new HistogramMetricValue(buckets));
        }
};

template <typename Server>
class ServerReplyPrinter: public Print {
    public:
        ServerReplyPrinter(Server & server) : server(server) {}

        size_t write(uint8_t c) override {
            server.sendContent(reinterpret_cast<const char *>(&c), 1);
            return 1;
        }

        size_t write(const uint8_t * buffer, size_t size) override {
            server.sendContent(reinterpret_cast<const char *>(buffer), size);
            return size;
        }

        Server & server;
};

class Registry: public Printable {
    public:
        Registry(const Labels & labels = {}): labels(labels) {}

        Registry(const Registry &) = delete;
        Registry & operator=(const Registry &) = delete;

        size_t printTo(Print & print) const final;

        template <typename Server>
        void register_metrics_endpoint(Server & server, const String & uri = "/metrics") {
            server.on(uri, [this, &server] {
                server.setContentLength(((size_t) -1));
                server.send(200, F("plain/text"), F(""));
                ServerReplyPrinter<Server> srp(server);
                printTo(srp);
            });
        }

        Labels labels;

    protected:
        std::set<Metric *> metrics;

        friend class Metric;
};

}
