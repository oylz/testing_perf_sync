#!/bin/bash

PPATH=`pwd`
function checkenv(){
    APPPATHNAME=$1
    APPPATH=$2
    
    CC=`echo $PATH | grep "$APPPATH" | wc -l`
    if [ $CC -eq 0 ]
    then
        echo "export $APPPATHNAME=$APPPATH" >> /etc/profile
        echo "export PATH=\$$APPPATHNAME/bin:\$PATH" >> /etc/profile
        source /etc/profile
    fi
}

function stop(){
    APP=$1
    ps aux | grep $APP | grep -v grep | awk '{print $2}' | xargs -i -t kill -9 {}
    rm /usr/local/$APP -rf
    rm /usr/local/pgsql -rf
}
function stopall(){
    stop postgres
    stop nredis
    stop nydbc
    stop nyrep
    stop nysync
    stop gsync
    stop java
}

Y_VERSION="latest"
checkenv JAVA_HOME /usr/local/jdk1.8.0_151
checkenv PGPATH /usr/local/pgsql
systemctl stop firewalld
systemctl disable firewalld

stopall

rm /build -rf
rm /root/logs/* -rf
mkdir /build
cp ./ss/* /build/

BASE_URL0="https://nexus.yealinkops.com/repository/packages-repo-develop"
BASE_URL="$BASE_URL0/cloud"


cd /build \
&& yum install wget -y \
&& yum install gperftools-devel gperftools -y \
&& wget $BASE_URL/pgsql/$Y_VERSION/linux/pgsql.tar.gz \
&& wget $BASE_URL/nysync/$Y_VERSION/linux/nysync.tar.gz \
&& wget $BASE_URL/nyrep/$Y_VERSION/linux/nyrep.tar.gz \
&& wget $BASE_URL/gsync/$Y_VERSION/linux/gsync.tar.gz \
&& wget $BASE_URL/nredis/$Y_VERSION/linux/nredis.tar.gz \
&& wget $BASE_URL/ymq/$Y_VERSION/linux/ymq.tar.gz \
&& wget $BASE_URL/nydbc/$Y_VERSION/linux/nydbc.tar.gz \
&& wget $BASE_URL0/team/framework/jdk/jdk-8u151-linux-x64.tar.gz \
&& wget $BASE_URL0/team/framework/rocketmq/latest/linux/rocketmq.tar.gz \
&& tar -xvf jdk-8u151-linux-x64.tar.gz -C /usr/local/ \
&& mkdir -p /usr/local/rocketmq \
&& tar -xvf rocketmq.tar.gz -C /usr/local/rocketmq/ \
&& cp /usr/local/rocketmq/conf/logback* /root/ \
&& tar -xvf pgsql.tar.gz -C /usr/local\
&& tar -xvf nyrep.tar.gz -C /usr/local \
&& tar -xvf nysync.tar.gz -C /usr/local \
&& tar -xvf gsync.tar.gz -C /usr/local \
&& tar -xvf nredis.tar.gz -C /usr/local \
&& tar -xvf ymq.tar.gz -C /usr/local \
&& cd /build \
&& tar -xvf nydbc.tar.gz -C /usr/local\
&& cp /build/sysctl.conf /etc/ \
&& sysctl -p \
&& rm /build/* -rf \
&& useradd postgres \

CC=`cat /etc/ld.so.conf | grep "pgsql/lib" | wc -l`
if [ $CC -eq 0 ]
then
    echo "/usr/local/pgsql/lib" >> /etc/ld.so.conf 
    ldconfig
fi

nf=`ulimit -n`
if [ $nf -lt 655350 ]
then
    FF=/etc/security/limits.conf
    echo "* hard memlock      unlimited" >> $FF
    echo "* soft memlock      unlimited" >> $FF
    echo "* hard nofile      655360" >> $FF
    echo "* soft nofile      655360" >> $FF
    echo "* hard nproc      unlimited" >> $FF
    echo "* soft nproc      unlimited" >> $FF
    echo "*******you must relogin, for limits.conf*******"
fi
 
#
DATA=/build/data
LOG=/build/log
mkdir -p $DATA
mkdir -p $LOG
chown -R postgres:postgres $DATA
chown -R postgres:postgres $LOG
ldconfig
# git cache 86400 seconds(1 day)
cd $PPATH
git config credential.helper 'cache --timeout=86400'
ulimit -c unlimited
echo "/tmp/core/core.%p" /proc/sys/kernel/core_pattern
mkdir -p /tmp/core





