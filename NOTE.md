chat_message.hpp
- `chat_message` maintaining the message data

server.hpp
- `chat_message_queue` == `deque<chat_message>`
- `chat_participant` is a abstract class
  - `virtual void deliver(const chat_message& msg)`
- `chat_participant_ptr` `shared_ptr<chat_participant>`
- chat_room
  - `participants_` 
  - `.join(participant)` paritcipant是shared_ptr<chat_participant>
  - `deliver(msg)` msg是chat_message
    - 把msg放到recent_msgs_(queue of history)
    - 但如果msg太多了我就把最前面的pop掉
    - for每個participant，call `deliver`，每個人都發一次訊息
- chat_session
  - 有socket
  - 有chat_room
  - session代表一個user，初始化的時候要給你所在的chat room，他會把你加入到蠟個chat room裡面，當傳輸出問題時，會把你從chat room kick掉
  - 當session發訊息時(server這邊read到時)，他會call chat room的deliver
    - chat room的recent_msg會shift一格
    - 向每個在chat room的user發送此訊息(這就是為啥這東西)

