#ifndef EVENT_TYPE_HPP
#define EVENT_TYPE_HPP

enum EventType {
    Server_Accepted_Connection,
    Server_Reject_Connection,
    ServerLoginAnnounce,
    ServerMessageBoardcast,
    ClientConnect,
    ClientDisconnect,
    ClientRegister,
    ClientSendMessage,
    Dummy = 99,
};

#endif // EVENT_TYPE_HPP
