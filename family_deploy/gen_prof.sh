#!/bin/bash

CC=$#
VER=$1

if [ $CC -lt 1 ]
then
    echo "usage:./gen_prof.sh ver"
    exit
fi


#echo nredis
#i=0
#for pid in `ps aux | grep redis | grep -v grep | grep cluster | awk '{print $2}'`
#do
#    let i=$i+1
#    if [ $i -gt 6 ]
#    then
#        break
#    fi
#    echo $pid
#    kill -s SIGINT $pid
#done
#
#echo nyrep
#let i=0
#for pid in `ps aux | grep nyrep | grep -v grep | awk '{print $2}'`
#do
#    echo $pid
#    kill -s SIGINT $pid
#    break; 
#done
#
#
#echo exphash
#for pid in `ps aux | grep redis | grep exphash | grep -v export | awk '{print $2}'`
#do
#    echo $pid
#    kill -s SIGINT $pid
#done


echo nysync
NYSYNC_COUNT=`cat nysync.count`
let i=0
for pid in `ps aux | grep nysync | grep -v grep | awk '{print $2}'`
do
    let i=$i+1
    if [ $i -gt $NYSYNC_COUNT ]
    then
        break
    fi
    echo $pid
    kill -s SIGINT $pid
done


echo put key to continue manually
read message
echo 1st char: ${message:0:1}



#pprof --pdf --nodefraction=0 --edgefraction=0 /usr/local/nyrep/bin/nyrep /build/log/nyrep.prof > nyrep"$VER".pdf
#pprof --text --nodefraction=0 --edgefraction=0 /usr/local/nyrep/bin/nyrep /build/log/nyrep.prof > nyrep"$VER".txt
#
#pprof --pdf --nodefraction=0 --edgefraction=0 /usr/local/nredis/bin/redis-server /build/log/nredis/4001/redis_vct_4001.prof > nyrep"$VER"_redis.pdf
#pprof --text --nodefraction=0 --edgefraction=0 /usr/local/nredis/bin/redis-server /build/log/nredis/4001/redis_vct_4001.prof > nyrep"$VER"_redis.txt
#
#pprof --pdf --nodefraction=0 --edgefraction=0 /usr/local/nredis/bin/exphash /build/log/nredis/4001/redis_vct_4001.prof.exphash.prof* > nredis"$VER"_exphash.pdf


if [ $NYSYNC_COUNT -eq 2 ]
then
    pprof --pdf --nodefraction=0 --edgefraction=0 /usr/local/nysync/bin/nysync /build/log/nysync1/nysync.prof > nysync"$VER"1.pdf
    pprof --pdf --nodefraction=0 --edgefraction=0 /usr/local/nysync/bin/nysync /build/log/nysync2/nysync.prof > nysync"$VER"2.pdf
else
    pprof --pdf --nodefraction=0 --edgefraction=0 /usr/local/nysync/bin/nysync /build/log/nysync1/nysync.prof > nysync"$VER".pdf
fi





