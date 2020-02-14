#!/bin/bash

PG_INSTALL_PATH=/usr/local/pgsql

rm bin -rf
mkdir bin

rm src/vctn.pb.* -rf

PPATH=`pwd`

cd dep
if !( test -f vctn.proto  )
then
    git clone --recursive http://gitcode.yealink.com/server/server_framework/nyrep.git
    cp nyrep/nredis/vct/src/vctn.proto ./
    rm nyrep -rf
fi


cd $PPATH
protoc --cpp_out=./src --proto_path=./dep/  ./dep/vctn.proto



g++ --std=c++11 -o bin/sync -I./src -I$PG_INSTALL_PATH/include -L$PG_INSTALL_PATH/lib src/main.cpp src/vctn.pb.cc -lglog -lboost_system -lhiredis -lffi -lprotobuf -lboost_thread -lpq





