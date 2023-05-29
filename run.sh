#!/usr/bin/bash

c++ -Wall -pthread -fcoroutines -std=c++20 -fconcepts-diagnostics-depth=3 -o phone Linphone.cpp Phone.cpp main.cpp -lfmt -lpigpio -lrt -lunifex 

sudo ./phone
