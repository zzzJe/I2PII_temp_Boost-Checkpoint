#include <cstdlib>
#include <deque>
#include <iostream>
#include <boost/bind/bind.hpp>
#include <boost/asio.hpp>
#include <boost/thread/thread.hpp>
#include "chat_message.hpp"
#include "event_type.hpp"
using namespace std;

using boost::asio::ip::tcp;

typedef std::deque<ChatMessage> chat_message_queue;

class ChatClient {
public:
    ChatClient(
        boost::asio::io_context& io_context,
        const tcp::resolver::results_type& endpoints
    ):
        io_context_(io_context),
        socket_(io_context)
    {
        // connect -> handle_connect() -> handle_read_header() -> handle_read_body()
        //                             -> handle_read_header() -> handle_read_body()
        //                             -> handle_read_header() -> handle_read_body() ...
        boost::asio::async_connect(
            socket_,
            endpoints,
            boost::bind(
                &ChatClient::handle_connect,
                this,
                boost::asio::placeholders::error
            )
        );
    }

    void write(const ChatMessage& msg) {
        boost::asio::post(
            io_context_,
            boost::bind(
                &ChatClient::do_write,
                this,
                msg
            )
        );
    }

    void close() {
        boost::asio::post(
            io_context_,
            boost::bind(
                &ChatClient::do_close,
                this
            )
        );
    }

    void try_register(const char* name) {
        ChatMessage msg(name, EventType::ClientRegister);
        write(msg);
    }

private:
    void handle_connect(const boost::system::error_code& error) {
        if (!error) {
            boost::asio::async_read(
                socket_,
                boost::asio::buffer(
                    read_msg_.data(),
                    ChatMessage::header_length
                ),
                boost::bind(
                    &ChatClient::handle_read_header,
                    this,
                    boost::asio::placeholders::error
                )
            );
        }
    }

    void handle_read_header(const boost::system::error_code& error) {
        if (!error && read_msg_.decode_header()) {
            // std::cout << "@handle_read_header: #body_length: " << read_msg_.body_length() << std::endl;
            if (read_msg_.type() == EventType::ServerLoginAnnounce) {

            }
            boost::asio::async_read(
                socket_,
                boost::asio::buffer(
                    read_msg_.body(),
                    read_msg_.body_length()
                ),
                boost::bind(
                    &ChatClient::handle_read_body,
                    this,
                    boost::asio::placeholders::error
                )
            );
        } else {
            do_close();
        }
    }

    void handle_read_body(const boost::system::error_code& error) {
        if (!error) {
            // std::cout << "@handle_read_body: #body: ";
            // for (int i = 0; i < read_msg_.body_length() ; i++) {
            //     std::cout << (int)read_msg_.body()[i] << " ";
            // }
            // std::cout << std::endl;

            if (read_msg_.type() == EventType::ServerLoginAnnounce) {
                std::cout << "* <";
                std::cout.write(read_msg_.body(), read_msg_.body_length());
                std::cout << "> join the server!\n";
            } else {
                int i = 0;
                for (; read_msg_.body()[i] != '\n'; i++) {
                    std::cout << read_msg_.body()[i]; 
                }
                std::cout << "> ";
                for (i++; i < read_msg_.body_length(); i++) {
                    std::cout << read_msg_.body()[i];
                }
                std::cout << "\n";
            }
            // repeated pattern
            boost::asio::async_read(
                socket_,
                boost::asio::buffer(
                    read_msg_.data(),
                    ChatMessage::header_length
                ),
                boost::bind(
                    &ChatClient::handle_read_header,
                    this,
                    boost::asio::placeholders::error
                )
            );
        } else {
            do_close();
        }
    }
    
    // do_write() -> handle_write() -> handle_write() -> handle_write() ... -> void
    void do_write(ChatMessage msg) {
        bool write_in_progress = !write_msgs_.empty();
        write_msgs_.push_back(msg);
        if (!write_in_progress) {
            boost::asio::async_write(
                socket_,
                boost::asio::buffer(
                    write_msgs_.front().data(),
                    write_msgs_.front().length()
                ),
                boost::bind(
                    &ChatClient::handle_write,
                    this,
                    boost::asio::placeholders::error
                )
            );
        }
    }

    void handle_write(const boost::system::error_code& error) {
        if (!error) {
            write_msgs_.pop_front();
            if (!write_msgs_.empty()) {
                boost::asio::async_write(
                    socket_,
                    boost::asio::buffer(
                        write_msgs_.front().data(),
                        write_msgs_.front().length()
                    ),
                    boost::bind(
                        &ChatClient::handle_write,
                        this,
                        boost::asio::placeholders::error
                    )
                );
            }
        } else {
            do_close();
        }
    }

    // close.
    void do_close() {
        socket_.close();
    }

public:
    const std::string& name() const {
        return name_;
    }

    void name(const std::string& name) {
        name_ = name;
    }

private:
    boost::asio::io_context& io_context_;
    tcp::socket socket_;
    ChatMessage read_msg_;
    chat_message_queue write_msgs_;
    std::string name_ = "dummy";
};

int main(int argc, char* argv[]) {
    try {
        if (argc != 3) {
            std::cerr << "Usage: ChatClient <host> <port>\n";
            return 1;
        }

        boost::asio::io_context io_context;

        tcp::resolver resolver(io_context);
        tcp::resolver::results_type endpoints = resolver.resolve(argv[1], argv[2]);

        std::string name;
        std::cout << "Your name: ";
        std::cin >> name;
        getchar();

        ChatClient c(io_context, endpoints);
        boost::thread t(boost::bind(&boost::asio::io_context::run, &io_context));

        c.name(name);
        c.try_register(c.name().c_str());

        char line[ChatMessage::max_body_length + 1];
        while (std::cin.getline(line, ChatMessage::max_body_length + 1)) {
            ChatMessage msg(line, EventType::ClientConnect);
            c.write(msg);
        }

        c.close();
        t.join();
    } catch (std::exception& e) {
        std::cerr << "Exception: " << e.what() << "\n";
    }

    return 0;
}