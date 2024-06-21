#include <algorithm>
#include <cstdlib>
#include <deque>
#include <iostream>
#include <list>
#include <set>
#include <boost/bind/bind.hpp>
#include <boost/shared_ptr.hpp>
#include <boost/enable_shared_from_this.hpp>
#include <boost/asio.hpp>
#include "chat_message.hpp"
using namespace std;
using boost::asio::ip::tcp;

//----------------------------------------------------------------------

typedef std::deque<ChatMessage> chat_message_queue;

//----------------------------------------------------------------------

class chat_participant {
public:
    virtual ~chat_participant() {}
    virtual void deliver(const ChatMessage& msg) = 0;
};

typedef boost::shared_ptr<chat_participant> chat_participant_ptr;

//----------------------------------------------------------------------

class ChatRoom {
public:
    void join(chat_participant_ptr participant) {
        participants_.insert(participant);
    }

    void leave(chat_participant_ptr participant) {
        participants_.erase(participant);
    }

    void deliver(const ChatMessage& msg) {
        std::for_each(
            participants_.begin(),
            participants_.end(),
            boost::bind(
                &chat_participant::deliver,
                boost::placeholders::_1,
                boost::ref(msg)
            )
        );
    }

private:
    std::set<chat_participant_ptr> participants_;
};

//----------------------------------------------------------------------

class ChatSession:
    public chat_participant,
    public boost::enable_shared_from_this<ChatSession>
{
public:
    ChatSession(boost::asio::io_context& io_context, ChatRoom& room):
        socket_(io_context),
        room_(room)
    {}

    tcp::socket& socket() {
        return socket_;
    }

    void start() {
        room_.join(shared_from_this());
        // read_msg_ <- header => handle_read_header()
        boost::asio::async_read(
            socket_,
            boost::asio::buffer(read_msg_.data(), ChatMessage::header_length),
            boost::bind(
                &ChatSession::handle_read_header, shared_from_this(),
                boost::asio::placeholders::error
            )
        );
    }

    void deliver(const ChatMessage& msg) {
        bool write_in_progress = !write_msgs_.empty();
        write_msgs_.push_back(msg);
        if (!write_in_progress) {
            // std::cout << "@session::deliver: #to_send_leng: " << write_msgs_.front().length() << std::endl;
            // std::cout << "@session::deliver: #to_send_data: |" << write_msgs_.front().data() << "|" << std::endl;
            boost::asio::async_write(
                socket_,
                boost::asio::buffer(
                    write_msgs_.front().data(),
                    write_msgs_.front().length()
                ),
                boost::bind(
                    &ChatSession::handle_write,
                    shared_from_this(),
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
                        &ChatSession::handle_write,
                        shared_from_this(),
                        boost::asio::placeholders::error
                    )
                );
            }
        } else {
            room_.leave(shared_from_this());
        }
    }

    void handle_read_header(const boost::system::error_code& error) {
        if (!error && read_msg_.decode_header()) {
            // std::cout << "@handle_read_header: #body_length: " << read_msg_.body_length() << std::endl;
            // @here header is decoded
            // read_msg_ <- body => handle_read_body()
            boost::asio::async_read(
                socket_,
                boost::asio::buffer(read_msg_.body(), read_msg_.body_length()),
                boost::bind(
                    &ChatSession::handle_read_body,
                    shared_from_this(),
                    boost::asio::placeholders::error
                )
            );
        } else {
            room_.leave(shared_from_this());
        }
    }

    void handle_read_body(const boost::system::error_code& error) {
        if (!error) {
            // std::cout << "@handle_read_body: #body: " << read_msg_.body() << std::endl;
            // trying to send msg to everyone in room
            if (read_msg_.type() == EventType::ClientRegister) {
                name_ = std::string(read_msg_.body());
                id_ = 0;
                std::string notification = name_;
                ChatMessage msg(notification.c_str(), EventType::ServerLoginAnnounce);
                room_.deliver(msg);
            } else {
                std::string notification = name_ + "\n";
                for (int i = 0; i < read_msg_.body_length(); i++) {
                    notification += read_msg_.body()[i];
                }
                ChatMessage msg(notification.c_str(), EventType::Dummy);
                room_.deliver(msg);
            }
            // read_msg_ <- header // this is repeated pattern
            boost::asio::async_read(
                socket_,
                boost::asio::buffer(read_msg_.data(), ChatMessage::header_length),
                boost::bind(
                    &ChatSession::handle_read_header,
                    shared_from_this(),
                    boost::asio::placeholders::error
                )
            );
        } else {
            room_.leave(shared_from_this());
        }
    }

    void fill_user_info(char* name, int id) {
        name_ = std::string(name);
        id_ = id;
    }

private:
    tcp::socket socket_;
    ChatRoom& room_;
    ChatMessage read_msg_;
    chat_message_queue write_msgs_;
    std::string name_;
    int id_;
};

typedef boost::shared_ptr<ChatSession> ChatSession_ptr;

//----------------------------------------------------------------------

class ChatServer {
public:
    ChatServer(
        boost::asio::io_context& io_context,
        const tcp::endpoint& endpoint
    ):
        io_context_(io_context),
        acceptor_(io_context, endpoint)
    {
        start_accept();
    }

    void start_accept() {
        ChatSession_ptr new_session(new ChatSession(io_context_, room_));
        acceptor_.async_accept(
            new_session->socket(),
            boost::bind(
                &ChatServer::handle_accept,
                this,
                new_session,
                boost::asio::placeholders::error
            )
        );
    }

    void handle_accept(
        ChatSession_ptr session,
        const boost::system::error_code& error
    ) {
        if (!error) {
            session->start();
        }

        start_accept();
    }

private:
    boost::asio::io_context& io_context_;
    tcp::acceptor acceptor_;
    ChatRoom room_;
};

typedef boost::shared_ptr<ChatServer> chat_server_ptr;
typedef std::list<chat_server_ptr> chat_server_list;

//----------------------------------------------------------------------

int main(int argc, char* argv[]) {
    try {
        if (argc < 2) {
            std::cerr << "Usage: ChatServer <port> [<port> ...]\n";
            return 1;
        }

        boost::asio::io_context io_context;

        chat_server_list servers;
        for (int i = 1; i < argc; ++i) {
            tcp::endpoint endpoint(tcp::v4(), atoi(argv[i]));
            chat_server_ptr server(new ChatServer(io_context, endpoint));
            servers.push_back(server);
        }

        io_context.run();
    } catch (std::exception& e) {
        std::cerr << "Exception: " << e.what() << "\n";
    }

    return 0;
}