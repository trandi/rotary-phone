#!/usr/bin/bash

c++ -Wall -pthread -fcoroutines -std=c++20 -fconcepts-diagnostics-depth=7 -o phone Phone.cpp main.cpp -lfmt -lpigpio -lrt -lunifex 

sudo ./phone
