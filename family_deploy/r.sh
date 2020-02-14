#!/bin/bash

REGION1_MQ_IP="10.120.10.201"
REGION2_MQ_IP="10.200.111.42"
GLOBAL_MQ_IP="10.200.110.167"
PEER_MQ_IP=""
IDC_FLAG=0
REGION=""
IS_GLOBAL=0
MQ_REGION_NM_SERVERS=""
MQ_NM_PORT=4008
NYSYNC_COUNT=`cat nysync.count`

IP=`ifconfig | grep inet | grep netmask | grep broadcast | grep -v 172 | awk '{print $2}'`


if [ "$IP" = $REGION1_MQ_IP ]
then
    echo "*******IN REGION1"
    PEER_MQ_IP=$REGION2_MQ_IP
    IDC_FLAG=0
    REGION="cn"
    IS_GLOBAL=0
elif [ "$IP" = $REGION2_MQ_IP ]
then
    echo "*******IN REGION2"
    PEER_MQ_IP=$REGION1_MQ_IP
    IDC_FLAG=1
    REGION="cn"
    IS_GLOBAL=0
else
    echo "*******IN GLOBAL"
    MQ_REGION_NM_SERVERS="$REGION1_MQ_IP:$MQ_NM_PORT;$REGION2_MQ_IP:$MQ_NM_PORT;"
    IS_GLOBAL=1
fi


DATA=/build/data
LOG=/build/log
PGPORT=4000
REDIS_BASE_PORT=4001
MQ_BR_PORT1=4009
MQ_BR_PORT2=4010
YMSG_PORT=4011
YMSGC_PORT=4012
NYDBC_PORT=4013
CHECK_DDL=0
YREP_SLOW_PORT=4014
SYNC_SLOW_PORT=4015
YMQ_PORT=4016
YEXT_PORT=4018
export log_info="no"


let REDIS_PORT1=$REDIS_BASE_PORT+1

# ====================================================================
# 1.postgres
function pg_append(){
    PGDATA=$1
    echo "bgwriter_delay = 10000ms" >> $PGDATA/postgresql.conf
    echo "bgwriter_flush_after = 256" >> $PGDATA/postgresql.conf
    echo "effective_io_concurrency = 64" >> $PGDATA/postgresql.conf
    echo "backend_flush_after = 256" >> $PGDATA/postgresql.conf
    echo "wal_writer_flush_after = 32MB" >> $PGDATA/postgresql.conf
    echo "checkpoint_flush_after = 256" >> $PGDATA/postgresql.conf
    sed -i "s/shared_buffers = 128MB/#xxxx shared_buffers = 128MB/g" $PGDATA/postgresql.conf
    echo "shared_buffers = 2GB" >> $PGDATA/postgresql.conf
    echo "effective_cache_size = 8GB" >> $PGDATA/postgresql.conf
    echo "work_mem = 8MB" >> $PGDATA/postgresql.conf
    echo "checkpoint_completion_target = 0.9" >> $PGDATA/postgresql.conf
    echo "wal_buffers = 32MB" >> $PGDATA/postgresql.conf
    echo "default_statistics_target = 100" >> $PGDATA/postgresql.conf
    echo "maintenance_work_mem = 256MB" >> $PGDATA/postgresql.conf
    echo "synchronous_commit = off" >> $PGDATA/postgresql.conf
    echo "full_page_writes  = off" >> $PGDATA/postgresql.conf
    echo "wal_sync_method = fdatasync" >> $PGDATA/postgresql.conf
    echo "commit_delay = 100000" >> $PGDATA/postgresql.conf
    echo "commit_siblings = 10" >> $PGDATA/postgresql.conf
    echo "autovacuum = off" >> $PGDATA/postgresql.conf
    echo "autovacuum_max_workers = 16" >> $PGDATA/postgresql.conf
    echo "hot_standby = off" >> $PGDATA/postgresql.conf
    echo "huge_pages = try" >> $PGDATA/postgresql.conf
    echo "fsync = off" >> $PGDATA/postgresql.conf
    echo "max_worker_processes = 16" >> $PGDATA/postgresql.conf
    echo "max_parallel_workers = 16" >> $PGDATA/postgresql.conf
    echo "track_activities = off" >> $PGDATA/postgresql.conf
    echo "track_counts = off" >> $PGDATA/postgresql.conf
    sed -i "s/max_wal_size = 1GB/#xxxx max_wal_size = 1GB/g" $PGDATA/postgresql.conf
    echo "max_wal_size = 256MB" >> $PGDATA/postgresql.conf
    sed -i "s/min_wal_size = 80MB/#min_wal_size = 80MB/g" $PGDATA/postgresql.conf
    echo "min_wal_size = 80MB" >> $PGDATA/postgresql.conf
}
function pp(){
    PGDATA=$1
    PGLOG=$2

    runuser -l postgres -c "/usr/local/pgsql/bin/initdb -D $PGDATA -E UTF8"
    echo "host    all             all             0.0.0.0/0            trust" >> $PGDATA/pg_hba.conf
    echo "host    replication             all             0.0.0.0/0            trust" >> $PGDATA/pg_hba.conf
    
    sed -i "s/#listen_addresses = 'localhost'/listen_addresses = '*'/g" $PGDATA/postgresql.conf
    sed -i "s/max_connections = 100/max_connections = 2200/g" $PGDATA/postgresql.conf
    
    echo "max_prepared_transactions = 2000" >>  $PGDATA/postgresql.conf
    # need by yrep
    echo "wal_level = logical" >>  $PGDATA/postgresql.conf
    echo "log_filename = 'postgresql-%Y-%m-%d_%H%M%S.log'"  >>  $PGDATA/postgresql.conf
    echo "logging_collector = on"  >>  $PGDATA/postgresql.conf
    echo "log_directory = 'PGLOGP'"  >>  $PGDATA/postgresql.conf
    NPGLOG=${PGLOG//\//\\\/}
    sed -i "s/PGLOGP/$NPGLOG/g" $PGDATA/postgresql.conf
    echo "log_min_messages = fatal" >>  $PGDATA/postgresql.conf
    echo "log_min_error_statement = fatal" >>  $PGDATA/postgresql.conf
    echo "port = $PGPORT" >>  $PGDATA/postgresql.conf
    echo "yrep_redis = '127.0.0.1:$REDIS_BASE_PORT,127.0.0.1:$REDIS_PORT1'" >>  $PGDATA/postgresql.conf
    echo "yrep_syncuser = 'sync'" >>  $PGDATA/postgresql.conf
    if [ $CHECK_DDL -eq 1 ]
    then
        echo "yrep_check_ddl = on" >>  $PGDATA/postgresql.conf
    fi 
    pg_append $PGDATA 

    runuser -l postgres -c "/usr/local/pgsql/bin/pg_ctl -D $PGDATA -l $PGLOG/pg_log.txt start"
    /usr/local/pgsql/bin/psql -h localhost -U postgres -c "CREATE USER sync WITH PASSWORD 'sync';" -p $PGPORT   
    /usr/local/pgsql/bin/psql -h localhost -U postgres -c "create database testdb;" -p $PGPORT   
    /usr/local/pgsql/bin/psql -h localhost -U postgres -d testdb -c "ALTER USER sync WITH SUPERUSER;" -p $PGPORT
    /usr/local/pgsql/bin/psql -h localhost -U postgres -d testdb -c "CREATE FUNCTION yfilter() RETURNS trigger AS 'yfilter' LANGUAGE C;" -p $PGPORT
    /usr/local/pgsql/bin/psql -h localhost -U postgres -d testdb  -p $PGPORT < /usr/local/pgsql/lib/trigger.sql
    /usr/local/pgsql/bin/psql -h localhost -U postgres -d testdb -c "SELECT pg_create_logical_replication_slot('yrep', 'ydec');" -p $PGPORT   
    # add test_decoding for debug
    /usr/local/pgsql/bin/psql -h localhost -U postgres -d testdb -c "SELECT pg_create_logical_replication_slot('tt', 'test_decoding');" -p $PGPORT   
}
pp $DATA $LOG

# ====================================================================
# 2.rocketmq
mkdir -p $DATA/mq
rm /usr/local/rocketmq/conf/logback* -rf
cp /root/logback* /usr/local/rocketmq/conf/
YY=${LOG//\//\\\/}
sed -i "s/\${user.home}/$YY/g" /usr/local/rocketmq/conf/logback*
    # nameserver
echo "listenPort=$MQ_NM_PORT" > $DATA/n.conf

str=$"\n"
nohup /usr/local/rocketmq/bin/mqnamesrv -c $DATA/n.conf > $LOG/name.log 2>&1 &
sstr=$(echo -e $str)
# broker
echo "namesrvAddr=0.0.0.0:$MQ_NM_PORT" > $DATA/b.conf
echo "brokerIP1=$IP" > $DATA/b.conf
echo "storePathRootDir=$DATA/mq/store" >> $DATA/b.conf
echo "storePathCommitLog=$DATA/mq/store/commitlog" >> $DATA/b.conf
echo "brokerClusterName=yyyy" >> $DATA/b.conf
echo "brokerName=bbbbb" >> $DATA/b.conf
echo "listenPort=$MQ_BR_PORT1" >> $DATA/b.conf
echo "haListenPort=$MQ_BR_PORT2" >> $DATA/b.conf
echo "brokerId=0" >> $DATA/b.conf
echo "maxMessageSize=10485760" >> $DATA/b.conf
echo "brokerRole=ASYNC_MASTER" >> $DATA/b.conf
echo "flushDiskType=ASYNC_FLUSH" >> $DATA/b.conf
echo "enablePropertyFilter=true" >> $DATA/b.conf
echo "waitTimeMillsInSendQueue=2000" >> $DATA/b.conf
echo "defaultTopicQueueNums=512" >> $DATA/b.conf
echo "sendMessageThreadPoolNums=256" >> $DATA/b.conf
echo "useReentrantLockWhenPutMessage=True" >> $DATA/b.conf
echo "osPageCacheBusyTimeOutMills=10000" >> $DATA/b.conf
echo "pullMessageThreadPoolNums=128" >> $DATA/b.conf
echo "sendThreadPoolQueueCapacity=100000" >> $DATA/b.conf
echo "pullThreadPoolQueueCapacity=100000" >> $DATA/b.conf
echo "serverWorkerThreads=64" >> $DATA/b.conf
echo "clientWorkerThreads=32" >> $DATA/b.conf
echo "clientCallbackExecutorThreads=64" >> $DATA/b.conf
echo "serverSelectorThreads=4" >> $DATA/b.conf
echo "maxTransferCountOnMessageInMemory=1024" >> $DATA/b.conf
echo "maxTransferBytesOnMessageInMemory=10485760" >> $DATA/b.conf
echo "serverSocketRcvBufSize=10485760" >> $DATA/b.conf
echo "clientSocketRcvBufSize=10485760" >> $DATA/b.conf
echo "serverSocketSndBufSize=10485760" >> $DATA/b.conf
echo "clientSocketSndBufSize=10485760" >> $DATA/b.conf
str=$"\n"
nohup /usr/local/rocketmq/bin/mqbroker -n localhost:$MQ_NM_PORT -c $DATA/b.conf > $LOG/broker.log 2>&1 &
sstr=$(echo -e $str)

while true
do
    /usr/local/rocketmq/bin/mqadmin updateTopic -n localhost:$MQ_NM_PORT -c yyyy -t sync
    C=`/usr/local/rocketmq/bin/mqadmin topicList -n localhost:$MQ_NM_PORT | grep "\<sync\>" | wc -l`
    echo C:$C
    if [ $C -lt 1 ]
    then
        echo "retry..."
        continue
    fi
    /usr/local/rocketmq/bin/mqadmin updateTopic -n localhost:$MQ_NM_PORT -c yyyy -t gsync
    /usr/local/rocketmq/bin/mqadmin updateTopic -n localhost:$MQ_NM_PORT -c yyyy -t check
    #/usr/local/rocketmq/bin/mqadmin updateTopic -n localhost:$MQ_NM_PORT -c yyyy -t ymq
    break
done

# 3.ymq
#nohup $JAVA_HOME/bin/java -jar /usr/local/ymq/bin/ymq.jar $IP $PGPORT 9016 $YMQ_PORT 100 localhost:$MQ_NM_PORT ymq_producer_group1 ymq_producer_instance1 1 > $LOG/ymq1.log 2>&1 &
#let YMQ_PORT2=$YMQ_PORT+1
#nohup $JAVA_HOME/bin/java -jar /usr/local/ymq/bin/ymq.jar $IP $PGPORT 9016 $YMQ_PORT2 100 localhost:$MQ_NM_PORT ymq_producer_group2 ymq_producer_instance2 1 > $LOG/ymq2.log 2>&1 &
# ====================================================================
LDP=$LD_LIBRARY_PATH

# 4.nredis
export LD_LIBRARY_PATH=/usr/local/nredis/lib:$LDP
RRSTR=""
function rr(){
    PPORT=$1
    IDC=$IDC_FLAG
    CLUSTER_STR=""
    for ((i=0;i<6;i++))
    do
        let PP=$PPORT+$i
        rm $DATA/nredis/$PP -rf
        mkdir -p $DATA/nredis/$PP
        mkdir -p $LOG/nredis/$PP
        CLUSTER_STR="$CLUSTER_STR $IP:$PP"
        if [ $i -eq 0 ]
        then
            RRSTR="$IP:$PP" 
        else
            RRSTR="$RRSTR,$IP:$PP" 
        fi

        CCF=$DATA/nredis/$PP/cc.conf
        echo "cluster-enabled yes" > $CCF
        echo "bind $IP 127.0.0.1" >> $CCF
        echo "appendonly yes" >> $CCF
        echo "appendfilename \"nydbc.aof\"" >> $CCF
        echo "aof_pos 1" >> $CCF
        echo "auto-aof-rewrite-percentage 0" >> $CCF
        echo "auto-aof-rewrite-min-size 1kb" >> $CCF
        echo "aof-use-rdb-preamble yes" >> $CCF
        echo "save \"\"" >> $CCF
        echo "rdbchecksum yes" >> $CCF
        echo "loadmodule /usr/local/nredis/bin/libnym.so" >> $CCF
        echo "port $PP" >> $CCF
        echo "logfile \"$LOG/nredis/$PP/log.txt\"" >> $CCF
        echo "vct_parall 2" >> $CCF
        echo "if_send_to_check 0" >> $CCF
        if [ $IS_GLOBAL -eq 0 ]
        then
            echo "ymsg-servers \"100,$IP:$YMSG_PORT\"" >> $CCF
            echo "region \"$REGION\"" >> $CCF
            echo "area \"$IDC\"" >> $CCF
            echo "idc_flag $IDC" >> $CCF
        fi

        cd $DATA/nredis/$PP
        export CPUPROFILE=$LOG/nredis/$PP/redis_vct_$PP.prof
        nohup /usr/local/nredis/bin/redis-server ./cc.conf > /dev/null 2>&1 &
        unset CPUPROFILE
    done
    /usr/local/nredis/bin/redis-cli --cluster create $CLUSTER_STR --cluster-replicas 1
}
rr $REDIS_BASE_PORT

# ====================================================================
# gsync
if [ $IS_GLOBAL -eq 1 ]
then
    export LD_LIBRARY_PATH=/usr/local/gsync/lib:$LDP
    export pg_servers=127.0.0.1:$PGPORT
    export redis_servers="127.0.0.1:$REDIS_BASE_PORT,127.0.0.1:$REDIS_PORT1"
    export log_path="/build/log"
    export port=$YMSGC_PORT
    export workcount=100
    export ymsgg_nameserver="127.0.0.1:$MQ_NM_PORT"
    export ymsgg_region_nameservers="$MQ_REGION_NM_SERVERS"
    export ymsgg_region_base_group=groupg
    export ymsgg_region_base_instance=instanceg
    export java_full_path="$JAVA_HOME/bin/java"
    export slow_port="$SYNC_SLOW_PORT"
    nohup /usr/local/gsync/bin/gsync /usr/local/gsync/conf/config.ini > $LOG/gsync.log 2>&1 &
    exit
fi
# ====================================================================
# 5.nyrep
export LD_LIBRARY_PATH=/usr/local/nyrep/lib:$LDP
export pg_servers="127.0.0.1:$PGPORT"
export redis_servers="127.0.0.1:$REDIS_BASE_PORT,127.0.0.1:$REDIS_PORT1"
export log_path="/build/log"
export ymsg_port="$YMSG_PORT"
export ymsg_nameserver="127.0.0.1:$MQ_NM_PORT"
export slow_port="$YREP_SLOW_PORT"
export java_full_path="$JAVA_HOME/bin/java"
export ymsg_workcount=1100
export ymsg_client_size=400
export pgbus_parall=256
export yext_port=$YEXT_PORT
export CPUPROFILE=$LOG/nyrep.prof
nohup /usr/local/nyrep/bin/nyrep /usr/local/nyrep/conf/config.ini > /dev/null 2>&1 &
unset CPUPROFILE

# ====================================================================
# 6.nysync
function nysync(){
    MSGC_PORT=$1
    SLOW_PORT=$2 
    instance=$3
    mkdir -p $LOG/$instance
    export LD_LIBRARY_PATH=/usr/local/nysync/lib:$LDP
    export pg_servers="127.0.0.1:$PGPORT"
    export redis_servers="127.0.0.1:$REDIS_BASE_PORT,127.0.0.1:$REDIS_PORT1"
    export log_path="$LOG/$instance"
    export port=$MSGC_PORT
    export workcount=100
    export ymsgc_peer_nameserver="$PEER_MQ_IP:$MQ_NM_PORT"
    export ymsgc_peer_group="groupp_"$REGION"_"$IDC_FLAG
    export ymsgc_peer_instance="instancep_"$REGION"_"$IDC_FLAG"_"$instance
    export ymsgc_global_nameserver="$GLOBAL_MQ_IP:$MQ_NM_PORT"
    export ymsgc_global_group="groupg_"$REGION"_"$IDC_FLAG
    export ymsgc_global_instance="instanceg_"$REGION"_"$IDC_FLAG"_"$instance
    let LP=$MSGC_PORT+20
    export JARGS="-Xms10g -Xmx10g -Xmn5g -Dcom.sun.management.jmxremote=true -Djava.rmi.server.hostname=$IP -Dcom.sun.management.jmxremote.port=$LP -Dcom.sun.management.jmxremote.ssl=false  -Dcom.sun.management.jmxremote.authenticate=false"
    export java_full_path="$JAVA_HOME/bin/java $JARGS"
    export region=$REGION
    export slow_port=$SLOW_PORT
    export CPUPROFILE=$LOG/$instance/nysync.prof
    nohup /usr/local/nysync/bin/nysync /usr/local/nysync/conf/config.ini > /dev/null 2>&1 &
    unset CPUPROFILE
}
nysync $YMSGC_PORT $SYNC_SLOW_PORT nysync1
if [ $NYSYNC_COUNT -eq 2 ]
then
    let YMSGC_PORT1=$YMSGC_PORT+10
    let SYNC_SLOW_PORT1=$SYNC_SLOW_PORT+10
    nysync $YMSGC_PORT1 $SYNC_SLOW_PORT1 nysync2
fi
export log_path="$LOG"
# ====================================================================

# 7.ydbc
export LD_LIBRARY_PATH=/usr/local/nydbc/lib:$LDP
export server_port="$NYDBC_PORT"
export postgres_connstr="127.0.0.1:$PGPORT"
export redis_conn_str="$RRSTR"
nohup /usr/local/nydbc/bin/nydbc /usr/local/nydbc/conf/config.ini > $LOG/nydbc.log 2>&1 &




