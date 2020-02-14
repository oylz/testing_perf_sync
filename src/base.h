#ifndef _BASEH_
#define _BASEH_
#include <sys/time.h>
#include <inttypes.h>
#include <string>
#include <vector>
#define GLOG_NO_ABBREVIATED_SEVERITIES
#include <glog/logging.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <string.h>
#include <boost/shared_ptr.hpp>
#include <boost/thread/mutex.hpp>
#include <boost/thread/thread.hpp>
#include <boost/function.hpp>
#include <signal.h>

static std::string tolower(std::string &s) {
    std::transform(s.begin(), s.end(), s.begin(), 
                   [](unsigned char c){ return std::tolower(c); } 
                  );
    return s;
}
static bool str_equal_case(const std::string &in1, const std::string &in2){
    std::string str1 = in1;
    std::string str2 = in2;
    tolower(str1);
    tolower(str2);
    return str1==str2;    
}

static int str_find(unsigned char *str, int strlen, unsigned char *sub, int sublen, int pos){
    if(pos<0 || pos>=strlen){
        LOG(ERROR) << "str_find error, strlen:" << strlen << ", sublen" << sublen << ", pos:" << pos;
        return -1;
    }
    const char *str1 = (const char*)str + pos;
    unsigned char * p = str + pos;
    bool found = false;
    for(int i = pos; i < strlen; i++){
        found = true;
        if(strlen-i-1 < sublen){
            return -1;
        } 
        for(int j = 0; j < sublen; j++){
            unsigned char *tmp1 = str + (i+j);
            unsigned char *tmp2 = sub + j;
            if(*tmp1 != *tmp2){
                found = false;
                break;
            }
        }
        if(found){
            return i;
        }
    }
    return -1;
}
static bool sub_str(unsigned char *buf, int cap, int pos, int len, std::string &out){
    if(cap<0 || pos<0 || len<0){
        LOG(ERROR) << "sub_str error-A, size:" << cap << ", pos:" << pos << ", len:" << len;
        return false;
    }
    if(cap < pos + len){
        LOG(ERROR) << "sub_str error-B, size:" << cap << ", pos:" << pos << ", len:" << len;
        return false;
    }  
    const char *buf1 = (const char*)(buf + pos);
    out = std::string(buf1, len);
    return true;
}
static void split_str(const std::string& input_str, const std::string &key, std::vector<std::string>& out_str_vec) {  
    if(input_str == ""){
        return;
    }
    int pos = input_str.find(key);
    int oldpos = -1;
    if(pos > 0){
        std::string tmp = input_str.substr(0, pos);
        out_str_vec.push_back(tmp);
    }
    while(1){
        if(pos < 0){
            break;
        }
        oldpos = pos;
        int newpos = input_str.find(key, pos + key.length());
        std::string tmp = input_str.substr(pos + key.length(), newpos - pos - key.length());
        out_str_vec.push_back(tmp);
        pos = newpos;
    }
    int tmplen = 0;
    if(out_str_vec.size() > 0){
        tmplen = out_str_vec.at(out_str_vec.size() - 1).length();
    }
    if(oldpos+tmplen < (int)input_str.length()-1){
        std::string tmp = input_str.substr(oldpos + key.length());
        out_str_vec.push_back(tmp);
    }
} 

static void replace_all(std::string &str, const std::string& from, const std::string& to) {
    size_t start_pos = 0;
    while((start_pos = str.find(from, start_pos)) != std::string::npos) {
        str.replace(start_pos, from.length(), to);
        start_pos += to.length();
    }
}

static void trim(std::string &s, const std::string &trimstr){  
    if (s.empty()){  
        return;  
    }  
    s.erase(0,s.find_first_not_of(trimstr));  
    s.erase(s.find_last_not_of(trimstr) + 1);  
}

static uint64_t gtm(){
    struct timeval tm;
    gettimeofday(&tm, 0);
    uint64_t re = ((uint64_t)tm.tv_sec)*1000*1000 + tm.tv_usec;
    return re;
}
static std::string gtmstr(){
    struct timeval tv;
    time_t nowtime;
    struct tm *nowtm;
    char tmbuf[64], buf[64];

    gettimeofday(&tv, NULL);
    nowtime = tv.tv_sec;
    nowtm = localtime(&nowtime);
    strftime(tmbuf, sizeof tmbuf, "%Y-%m-%d %H:%M:%S", nowtm);
    snprintf(buf, sizeof buf, "%s.%06ld", tmbuf, tv.tv_usec);
    return buf;
}
static std::string gtmstr(uint64_t inusec){
    time_t nowtime;
    struct tm *nowtm;
    char tmbuf[64], buf[64];
    uint64_t sec = inusec/1000000;
    uint64_t usec = inusec%1000000;
    nowtime = sec;
    nowtm = localtime(&nowtime);
    strftime(tmbuf, sizeof tmbuf, "%Y-%m-%d %H:%M:%S", nowtm);
    snprintf(buf, sizeof buf, "%s.%lu", tmbuf, usec);
    return buf;
}



struct redis_node_n{
    int aof_pos_;
    uint64_t fpos_;// pos in the file
    std::string cmd_;
    std::vector<std::string> args_;
    redis_node_n(int aof_pos, uint64_t fpos){
        aof_pos_ = aof_pos;
        fpos_ = fpos;
        cmd_ = "";
    }
    std::string to_string(){
        std::string str = cmd_;
        str += "(";
        str += std::to_string(aof_pos_);
        str += ":";
        str += std::to_string(fpos_);
        str += ")";
        for(const std::string &arg:args_){
            str += ", ";
            str += arg;
        }
        return str;
    }
    void print(bool is_master){
        std::string str = to_string();
        LOG(INFO) << "is_master:" << is_master << ", redis_node:[" << str << "]";
    }
};
typedef boost::shared_ptr<redis_node_n> redis_node;

static std::string get_exe_path(){
    std::string re = "";
    char exe_full_path[1024] = {0};
    char *last_slash = NULL;
    char exe_path[1024] = {0};

    // exe_path
    if(readlink("/proc/self/exe", exe_full_path,1024) <=0){
        LOG(ERROR) << "error to get self path";
        return re;
    }  
    last_slash = strrchr(exe_full_path, '/');
    if(last_slash == NULL){
        LOG(ERROR) << "error to find slash in exe_full_path";
        return re;
    }
    snprintf(exe_path,
        last_slash-exe_full_path+1,
        "%s",
        exe_full_path);
 
    re = std::string(exe_path);
    return re; 
}

struct pg_node_n{
    std::string lsns_;
    uint64_t lsn_;
    uint64_t xid_;
    std::string cmd_;
    std::string table_;
    std::string key_;
    std::vector<std::string> pkeys_;
    std::vector<std::pair<std::string, std::string> > cols_;
    pg_node_n(const std::string &lsns, uint64_t lsn, uint64_t xid){
        lsns_ = lsns;
        lsn_ = lsn;
        xid_ = xid;
        cmd_ = "";
        table_ = "";
    }
    std::string to_string(){
        std::string re = std::to_string(lsn_);
        re += ", ";
        re += std::to_string(xid_);
        re += ", ";
        re += cmd_;
        re += ", ";
        re += table_;
        re += ", ";
        re += key_;
        re += ", (";
        for(int i = 0; i < cols_.size(); i++){
            if(i > 0){
                re += ", ";
            }
            re += cols_[i].first;
            re += ":";
            re += cols_[i].second;
        }
        re += ")";
        return re;
    } 
};
typedef boost::shared_ptr<pg_node_n> pg_node;

typedef boost::function<bool (const pg_node &pn)> pg_node_fun;

static void usr1_signal_for_print(int sig){
    LOG(ERROR) << "usr1_signal_for_print";
}
static void set_usr1_signal_for_print(){
    signal(SIGUSR1, usr1_signal_for_print);
}
#endif




