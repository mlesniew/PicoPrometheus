#pragma once

#include <Arduino.h>

namespace PicoPrometheus {

template <size_t buffer_size = 256>
class BufferedPrint: public Print {
    public:
        BufferedPrint(Print & print): print(print), pos(0) {}
        ~BufferedPrint() { flush_buffer(); }

        size_t write(const uint8_t * src, size_t size) override {
            size_t written = 0;
            while (written < size) {
                const size_t remain = size - written;
                const size_t free_space = buffer_size - pos;
                const size_t chunk_size = free_space >= remain ? remain : free_space;
                memcpy(buffer + pos, src + written, chunk_size);
                pos += chunk_size;
                if (pos == buffer_size) {
                    flush_buffer();
                }
                written += chunk_size;
            }
            return written;
        }

        size_t write(uint8_t c) override {
            return write(&c, 1);
        }

    protected:
        Print & print;
        uint8_t buffer[buffer_size];
        size_t pos;

        void flush_buffer() {
            if (pos) {
                print.write(buffer, pos);
                pos = 0;
            }
        }
};

}
