#ifndef _YREPREDISCALLERH_
#define _YREPREDISCALLERH_
extern "C" {
    #include <hiredis/hiredis.h>
}
#include <ffi.h>
#include "vctn.pb.h"
#include "base.h"

struct vct_key_n{
    uint64_t posm_;
    uint64_t poss_;
    std::string key_;
    uint64_t persistent_time_;
    vct_key_n(){
        posm_ = 0;
        poss_ = 0;
        persistent_time_ = 0;
    }
};
typedef boost::shared_ptr<vct_key_n> vct_key;

struct vct_key_cmp{
    bool operator()(const vct_key &k1, const vct_key &k2) const{
        if(k1->posm_ < k2->posm_){
            return true;
        }
        else if(k1->posm_ == k2->posm_){
            if(k1->poss_ < k2->poss_){
                return true;
            }
        }
        return false;
    }
};

enum vct_oper{
    vo_none = 0,
    vo_up,
    vo_commit,
    vo_syncup,
    vo_synccommit,
    vo_gsyncup,
    vo_get
};

static std::string vct_oper_to_cmd(vct_oper vo){
    std::string cmd = "";
    switch(vo){
        case vo_up:
            cmd = "upvct";
            break;
        case vo_commit:
            cmd = "commitvct";
            break;
        case vo_syncup:
            cmd = "syncupvct";
            break;
        case vo_gsyncup:
            cmd = "gsyncupvct";
            break;
        case vo_synccommit:
            cmd = "synccommitvct";
            break;
        case vo_get:
            cmd = "getvct";
            break;
        default:
            LOG(ERROR) << "error vo:" << (int)vo;
            break;
    }
    return cmd;
}


static const std::string CLUSTERDOWN = "CLUSTERDOWN";
struct yrep_redis_caller_n{
private:
    std::string fixed_ip_;
    std::string ip_;
    int port_;
    redisContext *ctx_;
    bool reconnectif(redisReply *re){
        if(re){
            if (re->type != REDIS_REPLY_ERROR) {
                return false;
            }
            std::string error(re->str, re->len);
            LOG(WARNING) << "reconnectif:" << error;
            int pos = (int)error.find(" ");
            if(pos < 0){
                return false;
            } 
            std::string sub = error.substr(0, pos);
            if(sub!="MOVED" && sub!=CLUSTERDOWN){
                return false;
            }
            if(sub == "MOVED"){
                pos = error.rfind(" ");
                std::string host = error.substr(pos + 1);
                LOG(WARNING) << "move to host:" << host;
                pos = host.find(":"); 
                if(pos < 0){
                    LOG(ERROR) << "pos < 0";
                    return false;
                }
                std::string ip = host.substr(0, pos);
                std::string ports = host.substr(pos+1);
                ip_ = ip;
                if(!fixed_ip_.empty()){
                    ip_ = fixed_ip_;
                }
                port_ = std::stoi(ports);
            }
            else{
                LOG(ERROR) << "cluster is down, sleep 2s and than continue.";
                usleep(2*1000*1000);
            }
        }
        else if(ctx_->err == 3 ||//Server closed the connection
            ctx_->err == 1){//Server reset the connection
            // go to connect loop
            LOG(ERROR) << "Server internal error, ctx_->err:" << ctx_->err;
        } 
        else{
            return false;
        }

        if (ctx_) {
            if(re){
                freeReplyObject((void*)re);
            }
            redisFree(ctx_);
            ctx_ = NULL;
        }
        while(1){
            LOG(WARNING) << "connect...";
            bool tmp = connect();
            if(tmp){
                break;
            }
            usleep(1*1000*1000);
        }
        return true;
    }
    redisReply *redis_command(redisContext *c, const char *format, const std::vector<std::string> &inv) {
        redisReply *re = NULL;
    
        ffi_cif cif;
        int len = inv.size();
        void* args[len+2];
        ffi_type* arg_types[len+2];
    
    
        arg_types[0] = &ffi_type_pointer;
        arg_types[1] = &ffi_type_pointer;
        for(int i = 0; i < len; i++){
            int pos = i + 2;
            arg_types[pos] = &ffi_type_pointer;
        }
    
    
        if(ffi_prep_cif_var(&cif, FFI_DEFAULT_ABI, 2, len+2, &ffi_type_pointer, arg_types) != FFI_OK){
            fprintf(stderr, "error to ffi_prep\n");
            return re;
        }
    
        args[0] = &c;
        args[1] = &format;
    
        const char *in[inv.size()];
        for(int i = 0; i < len; i++){
            int pos = i+2;
            in[i] = inv[i].c_str();
            args[pos] = &in[i];
        }
    
        //ffi_arg res;
        ffi_call(&cif, FFI_FN(redisCommand), &re, args); 
        return re;
    }
    redisReply *redis_command_binary(redisContext *c, const char *format, const std::vector<std::string> &inv) {
        redisReply *re = NULL;
    
        ffi_cif cif;
        int len = inv.size();
        void* args[2*len+2];
        ffi_type* arg_types[2*len+2];
    
        arg_types[0] = &ffi_type_pointer;
        arg_types[1] = &ffi_type_pointer;
        for(int i = 0; i < len; i++){
            int pos1 = 2*i + 2;
            int pos2 = 2*i + 1 + 2;
            arg_types[pos1] = &ffi_type_pointer;
            arg_types[pos2] = &ffi_type_uint64;
        }
    
    
        if(ffi_prep_cif_var(&cif, FFI_DEFAULT_ABI, 2, 2*len+2, &ffi_type_pointer, arg_types) != FFI_OK){
            fprintf(stderr, "error to ffi_prep\n");
            return re;
        }
    
        args[0] = &c;
        args[1] = &format;
        const void *in[len];
        size_t lens[len];
        for(int i = 0; i < len; i++){
            int pos1 = 2*i+2;
            int pos2 = 2*i+1+2;
            in[i] = inv[i].data();
            lens[i] = inv[i].size();
            args[pos1] = &in[i];
            args[pos2] = &lens[i];
        }
        //ffi_arg res;
        ffi_call(&cif, FFI_FN(redisCommand), &re, args); 
        return re;
    }

public:
    yrep_redis_caller_n(const std::string &ip, int port, std::string fixed_ip=""){
        ip_ = ip;
        port_ = port;
        ctx_ = NULL;
        fixed_ip_ = fixed_ip;
    }
    ~yrep_redis_caller_n(){
        if(ctx_ != NULL){
            redisFree(ctx_);
        }
    }
    bool connect(){
        struct timeval tmval = {30, 500000};
        ctx_ = redisConnectWithTimeout(ip_.c_str(), port_, tmval);
        if (NULL==ctx_ || ctx_->err) {
            LOG(ERROR) << "connect to [" << ip_ << ":" << port_ << "] failed, err:" << (ctx_!=NULL)?std::to_string(ctx_->err):"";
            return false;
        }
        return true;
    }

    bool exec(const std::string &cmd0,
        const std::string &key, 
        const std::vector<std::string> &values){
        std::string cmd = cmd0;
        std::vector<std::string> ars(1+values.size());
        
        ars[0] = key;
        cmd += " %s";
        for(int i = 0; i < values.size(); i++){
            ars[i+1] = values[i];
            cmd += " %s";
        }

        redisReply *re = NULL;
        //LOG(INFO) << "cmd:" << cmd;
        while(1){
            re = (redisReply*)redis_command(ctx_, cmd.c_str(), ars);
            if(reconnectif(re)){
                continue;
            }
            break;
        }
        #if 0
        if(re->type != REDIS_REPLY_INTEGER){
            std::string error(re->str, re->len);
            LOG(WARNING) << re->type << ", eeeeeeeeeeeeeeeeeeee:" << error << ", ctx_->err" << ctx_->err;
            freeReplyObject((void*)re);
            return false;
        }
        #endif 
        freeReplyObject((void*)re);
        return true;
    }
    void exec_binary(const std::string &cmd, const std::vector<std::string> &args){
        std::string format = cmd;
        for(const std::string &ar:args){
            format += " %b";
        } 
        redisReply *re = NULL;
        LOG(INFO) << "format:" << format;
        while(1){
            re = (redisReply*)redis_command_binary(ctx_, format.c_str(), args);
            if(reconnectif(re)){
                continue;
            }
            break;
        }
        if(re->type == REDIS_REPLY_INTEGER){
            LOG(ERROR) << "in exec_binary format:" << format << ",re->integer:" << re->integer;
        }
        else{
            LOG(ERROR) << "in exec_binary format:" << format << ", re->type:" << re->type << ", re->str:" << re->str;
        }

        freeReplyObject((void*)re);
    }

    std::string get(const std::string &key){
        std::vector<std::string> ars(1);
        std::string cmd = "get %s";
        ars[0] = key;
        redisReply *re = NULL;
        while(1){
            re = (redisReply*)redis_command(ctx_, cmd.c_str(), ars);
            if(reconnectif(re)){
                continue;
            }
            break;
        }

        if(re->type != REDIS_REPLY_STRING){
            LOG(ERROR) << "error type:" << re->type;
            freeReplyObject((void*)re);
            return "";
        }
        
        std::string value = std::string(re->str, re->len);

        freeReplyObject((void*)re);
        return value;
    }
    std::string hgetex(const std::string &key, const std::string &hkey){
        std::vector<std::string> ars(2);
        std::string cmd = "hgetex %s %s";
        ars[0] = key;
        ars[1] = hkey;
        redisReply *re = NULL;
        while(1){
            re = (redisReply*)redis_command(ctx_, cmd.c_str(), ars);
            if(reconnectif(re)){
                continue;
            }
            break;
        }

        if(re->type != REDIS_REPLY_ARRAY){
            if(re->type == REDIS_REPLY_NIL){
                freeReplyObject((void*)re);
                return "";
            }
            LOG(ERROR) << "error type:" << re->type;
            freeReplyObject((void*)re);
            return "";
        }
        std::string value = "";
        for(int i = 0; i < (int)re->elements; i++){
            redisReply *tmp = re->element[i];
            if(tmp->type != REDIS_REPLY_STRING){
                if(tmp->type == REDIS_REPLY_NIL){
                    freeReplyObject((void*)re);
                    return "";
                }
                LOG(ERROR) << "error type:" << tmp->type;
                freeReplyObject((void*)re);
                return "";
            }
            value = std::string(tmp->str, tmp->len);
            break;
        }


        freeReplyObject((void*)re);
        return value;
    }
    std::string hget(const std::string &key, const std::string &hkey){
        std::vector<std::string> ars(2);
        std::string cmd = "hget %s %s";
        ars[0] = key;
        ars[1] = hkey;
        redisReply *re = NULL;
        while(1){
            re = (redisReply*)redis_command(ctx_, cmd.c_str(), ars);
            if(reconnectif(re)){
                continue;
            }
            break;
        }

        if(re->type != REDIS_REPLY_STRING){
            if(re->type == REDIS_REPLY_NIL){
                freeReplyObject((void*)re);
                return "";
            }
            LOG(ERROR) << "error type:" << re->type;
            freeReplyObject((void*)re);
            return "";
        }
        std::string value = "";

        value = std::string(re->str, re->len);

        freeReplyObject((void*)re);
        return value;
    }
    int exists(const std::string &key){
        std::vector<std::string> ars(1);
        std::string cmd = "exists %b";
        ars[0] = key;
        redisReply *re = NULL;
        while(1){
            re = (redisReply*)redis_command_binary(ctx_, cmd.c_str(), ars);
            if(reconnectif(re)){
                continue;
            }
            break;
        }

        if(re->type != REDIS_REPLY_INTEGER){
            LOG(ERROR) << "error type:" << re->type;
            freeReplyObject((void*)re);
            return 0;
        }
        int value = re->integer; 

        freeReplyObject((void*)re);
        return value;
    }
    void mix(const std::string &cmd0, const std::vector<std::string> args){
        std::vector<std::string> ars(1);
        std::string cmd = cmd0;
        for(int i = 0; i < args.size(); i++){
            cmd += " %b";
        }
        redisReply *re = NULL;
        while(1){
            re = (redisReply*)redis_command_binary(ctx_, cmd.c_str(), args);
            if(reconnectif(re)){
                continue;
            }
            break;
        }
        std::string disp = "return:[\n";
        if(re->type == REDIS_REPLY_INTEGER){
            disp += std::to_string(re->integer);
        }
        else if(re->type==REDIS_REPLY_STRING ||
            re->type==REDIS_REPLY_STATUS ||
            re->type==REDIS_REPLY_ERROR){
            disp += std::string(re->str, re->len);
        }
        else if(re->type == REDIS_REPLY_ARRAY){
            disp += "====================\n";
            for(int i = 0; i < (int)re->elements; i++){
                vctn::vct_value vctv;
                redisReply *tmp = re->element[i];
                std::string line = "";
                if(tmp->type == REDIS_REPLY_INTEGER){
                    line = std::to_string(tmp->integer);
                }
                else if(tmp->type==REDIS_REPLY_STRING ||
                    tmp->type==REDIS_REPLY_STATUS ||
                    tmp->type==REDIS_REPLY_ERROR){
                    line = std::string(tmp->str, tmp->len);
                }
                disp += line;
                disp += "\n";
                if(i < re->elements-1){
                    disp += "--------------\n";
                }
            }
            disp += "====================\n";
        }
        else{
            disp += "none";
        }
        disp += "\n]";
        {
            FILE *fl = fopen("./out", "wb");
            fwrite(disp.data(), 1, disp.size(), fl);
            fclose(fl);
        }
        LOG(ERROR) << disp;

        freeReplyObject((void*)re);
    }

    void callvct(const std::vector<vct_key> &keys, std::vector<vctn::vct_value> &vctvs,
        vct_oper vo){
        std::string cmd = vct_oper_to_cmd(vo);
        if(cmd.empty()){
            return;
        }
        std::vector<std::string> ars;
        for(vct_key key:keys){
            cmd += " %s";
            ars.push_back(key->key_);
            cmd += " %s";
            ars.push_back(std::to_string(key->posm_));
            cmd += " %s";
            ars.push_back(std::to_string(key->poss_));
            if(vo == vo_up){
                cmd += " %s";
                ars.push_back(std::to_string(key->persistent_time_));               
            }
        } 
        redisReply *re = NULL;
        LOG(INFO) << "cmd:" << cmd;
        while(1){
            re = (redisReply*)redis_command(ctx_, cmd.c_str(), ars);
            if(reconnectif(re)){
                continue;
            }
            break;
        }

        if(re->type != REDIS_REPLY_ARRAY){
            std::string error(re->str, re->len);
            LOG(ERROR) << re->type << ", eeeeeeeeeeeeeeeeeeee:" << error << ", ctx_->err" << ctx_->err;
            freeReplyObject((void*)re);
            return;
        }
       
        if(re->elements%4 != 0){
            freeReplyObject((void*)re);
            return;
        } 
        for(int i = 0; i < (int)re->elements; i+=4){
            vctn::vct_value vctv;
            redisReply *tmp = re->element[i];
            if(tmp->type != REDIS_REPLY_STRING){
                LOG(ERROR) << "208 type:" << tmp->type;
                freeReplyObject((void*)re);
                return;
            } 
            std::string key(tmp->str, tmp->len);
            vctv.set_key_(key);
            vctn::vct_value_n *main = vctv.mutable_main_();
            for(int n = 1; n < 4; n++){
                tmp = re->element[i+n];
                if(tmp->type != REDIS_REPLY_INTEGER){
                    LOG(ERROR) << "217 type:" << tmp->type;
                    freeReplyObject((void*)re);
                    return;
                } 
                uint64_t pos = (uint64_t)tmp->integer;
                if(n == 1){
                    main->set_a_(pos);
                    continue;
                }
                if(n == 2){
                    main->set_b_(pos);
                    continue;
                }
                main->set_commit_(pos);
            }
            vctvs.push_back(vctv);
        }

        freeReplyObject((void*)re);
    }
};
typedef boost::shared_ptr<yrep_redis_caller_n> yrep_redis_caller;


#endif


