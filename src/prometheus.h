#ifndef PROMETHEUS_H
#define PROMETHEUS_H

#include <functional>
#include <map>
#include <memory>
#include <set>
#include <string>

#include <Arduino.h>

#ifdef ESP8266
#include <ESP8266WebServer.h>
#endif

using PrometheusWriter = std::function<void(const char *)>;
using PrometheusLabels = std::map<std::string, std::string>;

class PrometheusDumpable {
    public:
        virtual ~PrometheusDumpable() {}
        virtual void dump(PrometheusWriter) const = 0;
        void dump(Print & print) const;
#ifdef ESP8266
        void register_metrics_endpoint(ESP8266WebServer & server, const Uri & uri = "/metrics");
#endif
};

class Prometheus;
class PrometheusMetric;

class PrometheusMetricValue {
    public:
        PrometheusMetricValue() = default;
        PrometheusMetricValue(const PrometheusMetricValue &) = delete;
        PrometheusMetricValue & operator=(const PrometheusMetricValue &) = delete;

    protected:
        virtual void dump(PrometheusWriter fn, const std::string & name, const PrometheusLabels & labels) const = 0;

        friend class PrometheusMetric;
};

class PrometheusMetric: public PrometheusDumpable {
    public:
        PrometheusMetric(Prometheus & prometheus, const std::string & name, const std::string & help);
        virtual ~PrometheusMetric();

        PrometheusMetric(const PrometheusMetric &) = delete;
        PrometheusMetric & operator=(const PrometheusMetric &) = delete;

        void dump(PrometheusWriter fn) const final;

        const std::string name;
        const std::string help;

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
        void dump(PrometheusWriter fn, const std::string & name, const PrometheusLabels & labels) const override;

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

class Prometheus: public PrometheusDumpable {
    public:
        Prometheus() = default;

        Prometheus(const Prometheus &) = delete;
        Prometheus & operator=(const Prometheus &) = delete;

        void dump(PrometheusWriter write) const override;

    protected:
        std::set<PrometheusMetric *> metrics;

        friend class PrometheusMetric;
};

#endif
