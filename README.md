
**sync模块性能测试**

# 0.base command

* query count
```
psql -h pa1 -U postgres -d testdb -p 4000 -c "select count(*) from conferencerecord;";psql -h pa0 -U postgres -d testdb -p 4000 -c "select count(*) from conferencerecord;"
```

* mq tps
```
/usr/local/rocketmq/bin/mqadmin statsAll -n localhost:4008
```


# 1.src

## 1.1.npg

[nyrep](./nyrep.md)

## 1.2.nredis

[exphash](./exphash.md)


# 2.target

[nysync](./nysync.md)

# 3.[bug](./bug.md)












