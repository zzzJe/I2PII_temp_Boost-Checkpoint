#ifndef EVENT_TYPE_HPP
#define EVENT_TYPE_HPP

enum EventType {
    ClientConnect,
    ClientDisconnect,
    ServerAccept,
    ServerReject,
    ClientReady,
    ServerGameStart,
    ClinetFinish,
    ServerRoundTimeout,
    ServerDisplay
};

#endif // EVENT_TYPE_HPP
