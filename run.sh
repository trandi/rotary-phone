#!/usr/bin/bash

#g++ -Wall -pthread -std=c++20 -fconcepts-diagnostics-depth=3 -o phone Linphone.cpp Phone.cpp main.cpp -lfmt -lpigpio -lrt -lunifex 

clang++ -Wall -pthread -std=c++20 -o phone Phone.cpp main.cpp -lfmt -lpigpio -lrt -lunifex 

# kill previous process if necessary
# sudo kill -9 $(sudo netstat -ltnpa | grep 8888 | awk '{split($7,a,"/"); print a[1];}')
sudo kill -9 $(ps -ef | grep linphonec | awk 'NR==1{print $2;}')

sudo ./phone
