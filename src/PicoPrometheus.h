#ifndef PROMETHEUS_H
#define PROMETHEUS_H

#include <functional>
#include <map>
#include <memory>
#include <set>
#include <vector>

#include <Arduino.h>


class PrometheusLabels: public std::map<String, String> {
    public:
        using std::map<String, String>::map;
        bool is_subset_of(const PrometheusLabels & other) const;
};

class Prometheus;
class PrometheusMetric;

class PrometheusMetricValue {
    public:
        PrometheusMetricValue() = default;
        PrometheusMetricValue(const PrometheusMetricValue &) = delete;
        PrometheusMetricValue & operator=(const PrometheusMetricValue &) = delete;

    protected:
        virtual size_t printTo(Print & print, const String & name, const PrometheusLabels & global_labels, const PrometheusLabels & labels) const = 0;

        friend class PrometheusMetric;
};

class PrometheusMetric: public Printable {
    public:
        PrometheusMetric(Prometheus & prometheus, const String & name, const String & help);
        virtual ~PrometheusMetric();

        PrometheusMetric(const PrometheusMetric &) = delete;
        PrometheusMetric & operator=(const PrometheusMetric &) = delete;

        size_t printTo(Print & print) const final;

        void remove(const PrometheusLabels & labels, bool exact_match = true);
        void clear();

        const String name;
        const String help;

    protected:
        PrometheusMetricValue & get(const PrometheusLabels & labels);

        virtual std::unique_ptr<PrometheusMetricValue> construct_value() const = 0;
        virtual const char * get_prometheus_type_name() const = 0;

    private:
        std::map<PrometheusLabels, std::unique_ptr<PrometheusMetricValue>> metrics;
        Prometheus & prometheus;
};


template <typename T>
class PrometheusTypedMetric: public PrometheusMetric {
    public:
        using PrometheusMetric::PrometheusMetric;

        T & operator[](const PrometheusLabels & labels) {
            return static_cast<T &>(get(labels));
        }

        T & get_default_metric() { return (*this)[ {}]; }

    protected:
        std::unique_ptr<PrometheusMetricValue> construct_value() const override {
            return std::unique_ptr<PrometheusMetricValue>(new T());
        }
};


class PrometheusSimpleMetricValue: public PrometheusMetricValue {
    public:
        PrometheusSimpleMetricValue(): value(0) {}

    protected:
        size_t printTo(Print & print, const String & name, const PrometheusLabels & global_labels, const PrometheusLabels & labels) const override;

        double value;
};

class PrometheusGaugeValue: public PrometheusSimpleMetricValue {
    public:
        using PrometheusSimpleMetricValue::PrometheusSimpleMetricValue;
        void set(double value) { this->value = value; }
};

class PrometheusCounterValue: public PrometheusSimpleMetricValue {
    public:
        using PrometheusSimpleMetricValue::PrometheusSimpleMetricValue;
        void increment(double value = 1.0) { if (value > 0.0) this->value += value; }
};

class PrometheusGauge: public PrometheusTypedMetric<PrometheusGaugeValue> {
    public:
        using PrometheusTypedMetric<PrometheusGaugeValue>::PrometheusTypedMetric;
        void set(double value) { get_default_metric().set(value); }

    protected:
        const char * get_prometheus_type_name() const override { return "gauge"; }
};

class PrometheusCounter: public PrometheusTypedMetric<PrometheusCounterValue> {
    public:
        using PrometheusTypedMetric<PrometheusCounterValue>::PrometheusTypedMetric;
        void increment(double value = 1.0) { get_default_metric().increment(value); }

    protected:
        const char * get_prometheus_type_name() const override { return "counter"; }
};

class PrometheusHistogramMetricValue: public PrometheusMetricValue {
    public:
        static const std::vector<double> defalut_buckets;

        PrometheusHistogramMetricValue(const std::vector<double> & buckets = PrometheusHistogramMetricValue::defalut_buckets);

        void observe(double value);

    protected:
        size_t printTo(Print & print, const String & name, const PrometheusLabels & global_labels, const PrometheusLabels & labels) const override;

        unsigned long count;
        std::map<double, unsigned long> buckets;
        double sum;
};

class PrometheusHistogram: public PrometheusTypedMetric<PrometheusHistogramMetricValue> {
    public:
        PrometheusHistogram(Prometheus & prometheus, const String name, const String help,
                            const std::vector<double> & buckets = PrometheusHistogramMetricValue::defalut_buckets)
            : PrometheusTypedMetric<PrometheusHistogramMetricValue>(prometheus, name, help), buckets(buckets) {}

        void observe(double value) { get_default_metric().observe(value); }

        const std::vector<double> buckets;

    protected:
        const char * get_prometheus_type_name() const override { return "histogram"; }

        std::unique_ptr<PrometheusMetricValue> construct_value() const override {
            return std::unique_ptr<PrometheusMetricValue>(new PrometheusHistogramMetricValue(buckets));
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

class Prometheus: public Printable {
    public:
        Prometheus(const PrometheusLabels & labels = {}): labels(labels) {}

        Prometheus(const Prometheus &) = delete;
        Prometheus & operator=(const Prometheus &) = delete;

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

        PrometheusLabels labels;

    protected:
        std::set<PrometheusMetric *> metrics;

        friend class PrometheusMetric;
};

#endif
