#include "yrep_redis_caller.h"
#include "ini_config.h"
#include "ypg.h"
boost::shared_ptr<ini_config> ini_config::self_;

class perf_redis_n{
private:
    yrep_redis_caller caller_;
    std::string tag_;
    int count_;
public:
    perf_redis_n(){
        count_ = 0;
    }
    bool init(const std::string &tag, int count){
        tag_ = tag;
        count_ = count;

        std::string ips = "redis.ip";
        std::string ports = "redis.port";

        std::string ip = ini_config::instance()->get_str(ips);
        int port = ini_config::instance()->get_int(ports);
        caller_.reset(new yrep_redis_caller_n(ip, port));
        if(!caller_->connect()){
            caller_.reset();
            LOG(ERROR) << "connect to " << ip << ":" << port << "error, pass it";
            return false;
        }
        return true;
    }
    void run(){
        for(int i = 0; i < count_; i++){
            std::vector<std::string> ars;
            std::string f = "ffff"; f += std::to_string(i);ars.push_back(f);
            std::string v = "vvvv"; v += std::to_string(i);ars.push_back(v);
            
            uint64_t ms = gtm()/1000;
            ms += 5*60*1000;
            ars.push_back(std::to_string(ms));
            caller_->exec("hsetex", tag_, ars);
        }
    }
};
typedef boost::shared_ptr<perf_redis_n> perf_redis;

void pg_test(const std::string &users, const std::string &pwds, const std::string &ips, const std::string &ports){
        std::string user = users;
        std::string pwd = pwds;
        std::string ip = ips;
        std::string port = ports;
        std::string db = "testdb";

        ypg pg(new ypg_n(user, pwd, ip, port, db));
        if(!pg->connect()){
            pg.reset();
            LOG(ERROR) << "connect to " << ip << ":" << port << "error, pass it";
            return;
        }
        pg->exec("select 1;");
        LOG(ERROR) << "sucess!";
}

int main(int argc, char **argv){
    #if 0
    {
        std::vector<vct_key> vctkeys;
        for(int i = 0; i < 403; i++){
            vct_key vctkey(new vct_key_n());
            vctkey->key_ = "redis_{tag811}_ffff";
            //vctkey->key_ = "{tag811}";
            vctkey->key_ += std::to_string(i);
        
            vctkey->posm_ = 99;
            vctkey->poss_ = 99;
            uint64_t tm = 99;
            tm <<= 32;
            tm += 99;
 
            vctkey->persistent_time_ = tm;
            vctkeys.push_back(vctkey);
        }
        std::vector<vctn::vct_value> vctvs(vctkeys.size());
        yrep_redis_caller caller(new yrep_redis_caller_n("mq", 7001));
        if(!caller->connect()){
            fprintf(stderr, "caller connect error\n");
            return 0;
        } 
        caller->callvct(vctkeys, vctvs, vct_oper::vo_up);
 
        return 0;


        uint64_t tm = gtm();
        tm /= 1000;
        tm += 30*1000;
        fprintf(stderr, "%lu\n", tm);
        return 0;

        pg_test(argv[1], argv[2], argv[3], argv[4]);
        return 0;
    }
    #endif

    std::string pre = get_exe_path();
    pre += "/synclog/";
    fprintf(stderr, "pre:%s\n", pre.c_str());
    mkdir(pre.c_str(), S_IRWXU|S_IRWXG|S_IROTH|S_IXOTH);


    FLAGS_log_dir = pre.c_str();
    FLAGS_max_log_count = 4;
    google::InitGoogleLogging("sync");

    std::string path = pre;
    path += "INFO_";
    google::SetLogDestination(google::GLOG_INFO, path.c_str());

    path = pre;
    path += "ERROR_";
    google::SetLogDestination(google::GLOG_ERROR, path.c_str());

    path = pre;
    path += "WARNING_";
    google::SetLogDestination(google::GLOG_WARNING, path.c_str());
    FLAGS_colorlogtostderr = true;
    FLAGS_logbufsecs = 0;//30;
    FLAGS_max_log_size = 1024;
    FLAGS_stop_logging_if_full_disk = true;
    FLAGS_v = -1;

    ini_config::instance()->init(argv[1]);
    LOG(ERROR) << "begin";
    int N = 1000;
    int count = 10000;
    std::vector<perf_redis> prs;
    for(int i = 0; i < N; i++){
        perf_redis pr(new perf_redis_n());
        std::string tag = "tag";
        tag += std::to_string(i);
        pr->init(tag, count);
        prs.push_back(pr);
    }
    std::vector<boost::shared_ptr<boost::thread> > ths;
    for(int i = 0; i < N; i++){
        boost::shared_ptr<boost::thread> th(new boost::thread(
            boost::bind(&perf_redis_n::run, prs[i].get())
        ));
        ths.push_back(th);
    }
    for(int i = 0; i < N; i++){
        ths[i]->join();
    }

    LOG(ERROR) << "finish";
    google::ShutdownGoogleLogging();
    return 0;
}






