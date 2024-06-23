g++ -std=c++14 server.cpp -o server -lboost_system
echo "Server start listen on port 3000"
./server 3000
