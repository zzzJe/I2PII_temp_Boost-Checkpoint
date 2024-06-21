#ifndef CHAT_MESSAGE_HPP
#define CHAT_MESSAGE_HPP

#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <iomanip>
#include "event_type.hpp"

class ChatMessage {
public:
    enum {
        header_length_section = 4,
        header_type_section = 2,
        header_length = 6,
        max_body_length = 512
    };

    ChatMessage():
        body_length_(0)
    {}

    ChatMessage(const char* msg_str, EventType type):
        body_length_(strlen(msg_str)),
        type_(type)
    {
        memcpy(body(), msg_str, body_length());
        encode_header();
    }

    const char* data() const {
        return data_;
    }

    char* data() {
        return data_;
    }

    size_t length() const {
        return header_length + body_length_;
    }

    const char* body() const {
        return data_ + header_length;
    }

    char* body() {
        return data_ + header_length;
    }

    size_t body_length() const{
        return body_length_;
    }

    void body_length(size_t new_length) {
        body_length_ = new_length;
        if (body_length_ > max_body_length)
            body_length_ = max_body_length;
    }

    bool decode_header() {
        using namespace std; // For strncat and atoi.
        char first_part[5] = {0};
        strncpy(first_part, data_, header_length_section);
        int length_part = atoi(first_part);

        char second_part[3] = {0};
        strncat(second_part, data_ + header_length_section, header_type_section);
        EventType type_part = static_cast<EventType>(atoi(second_part));

        body_length_ = length_part;
        type_ = type_part;
        if (body_length_ > max_body_length) {
            body_length_ = 0;
            return false;
        }
        return true;
    }

    void encode_header() {
        using namespace std; // For sprintf and memcpy.
        ostringstream oss;
        oss << setw(header_length_section) << right << body_length_;
        oss << setw(header_type_section)   << right << type_;
        // std::cout << "@encode_header: #encode_result: " << "[" << oss.str().c_str() << "]\n";
        memcpy(data_, oss.str().c_str(), header_length);
    }

    EventType type() const {
        return type_;
    }

private:
    char data_[header_length + max_body_length];
    EventType type_;
    size_t body_length_;
};

#endif // CHAT_MESSAGE_HPP