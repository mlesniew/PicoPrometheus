#pragma once

#include <functional>
#include <map>
#include <memory>
#include <mutex>
#include <set>
#include <vector>

#include <Arduino.h>

#include "BufferedPrint.h"


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
    protected:
        size_t printTo(Print & print, const String & name, const Labels & global_labels, const Labels & labels) const override;
        virtual double get_value() const = 0;
};

class GaugeValue: public SimpleMetricValue {
    public:
        GaugeValue(): value(0), getter(nullptr) {}
        virtual double get_value() const override { return getter ? getter() : value; }

        void set(double value) { this->value = value; }

        void bind(const std::function<double()> & getter) { this->getter = getter; }

        template <typename T, typename = typename std::enable_if<!std::is_arithmetic<T>::value, T>::type>
        void bind(T getter) { this->getter = [getter]{ return (double) getter(); }; }

        template <typename T, typename = typename std::enable_if<std::is_arithmetic<T>::value, T>::type>
        void bind(const T & value) { this->getter = [&value]{ return (double) value; }; }

    protected:
        double value;
        std::function<double()> getter;
};

class CounterValue: public SimpleMetricValue {
    public:
        CounterValue(): value(0) {}
        void increment(double value = 1.0) { if (value > 0.0) this->value += value; }
        virtual double get_value() const override { return value; }

    protected:
        double value;
};

class Gauge: public TypedMetric<GaugeValue> {
    public:
        using TypedMetric<GaugeValue>::TypedMetric;

        template <typename T>
        Gauge(Registry & registry, const String & name, const String & help, T v)
            : Gauge(registry, name, help) {
            bind(v);
        }

        void set(double value) { get_default_metric().set(value); }

        template <typename T>
        void bind(T v) { get_default_metric().bind(v); }

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

        size_t write(const uint8_t * buffer, size_t size) override {
            server.sendContent(reinterpret_cast<const char *>(buffer), size);
            return size;
        }

        size_t write(uint8_t c) override {
            return write(&c, 1);
        }

        Server & server;
};

class Registry: public Printable {
    public:
        Registry(const Labels & labels = {}): labels(labels) {}

        Registry(const Registry &) = delete;
        Registry & operator=(const Registry &) = delete;

        size_t printTo(Print & print) const override;

        template <typename Server>
        void register_metrics_endpoint(Server & server, const String & uri = "/metrics") {
            server.on(uri, [this, &server] {
                server.setContentLength(((size_t) -1));
                server.send(200, F("plain/text"), F(""));
                ServerReplyPrinter<Server> srp(server);
                BufferedPrint<1024> bp(srp);
                printTo(bp);
            });
        }

        Labels labels;

    protected:
        std::set<Metric *> metrics;
        friend class Metric;
};

template <typename LockType>
class SynchronizedRegistry: public Registry {
    public:
        SynchronizedRegistry(const Labels & labels, LockType & lock): Registry(labels), lock(lock) {}
        SynchronizedRegistry(LockType & lock): Registry(), lock(lock) {}

        size_t printTo(Print & print) const override {
            std::lock_guard<LockType> guard(lock);
            return Registry::printTo(print);
        }

    protected:
        LockType & lock;
};

}
