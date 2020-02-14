#ifndef _INICONFIGH_
#define _INICONFIGH_

#include "base.h"
#include <exception>


class ini_config{
public:
    ~ini_config() {
    }
    static boost::shared_ptr<ini_config> instance(){
        if(self_.get() == NULL){
            self_ = boost::shared_ptr<ini_config>(new ini_config());
        }
        return self_;
    }
    bool init(const std::string conffile){
        std::string content;
        read(conffile, content);
        LOG(INFO) << "content:" << content.c_str();
        std::vector<std::string> lines;
        split_str(content, "\n", lines);
        for(const std::string &line:lines){
            std::string nline = line;
            trim(nline, " ");
            if(nline.empty()){
                continue;
            }
            char first = nline.at(0);
            if(first == '#'){
                continue;
            }
            int pos = (int)nline.find("=");
            if(pos < 0){
                continue;
            } 
            std::string name = nline.substr(0, pos);
            std::string value = nline.substr(pos+1);
            trim(name, " ");
            trim(value, " ");
            if(name.empty() || value.empty()){
                continue;
            }
            std::map<std::string, std::string>::iterator it = map_.find(name);
            if(it != map_.end()){
                LOG(WARNING) << "config name:" << name << ", has been config pre";
                continue;
            }
            map_.insert(std::make_pair(name, value));
        }
        return true;
    }
public:
    std::string get_str(const std::string &key){
        std::string re = "";
        std::map<std::string, std::string>::iterator it = map_.find(key);
        if(it == map_.end()){
            return re;
        }
        re = it->second;
        return re;
    }
    int get_int(const std::string &key){
        int re = 0;
        std::string res = this->get_str(key);
        if(res.empty()){
            return re;
        }
        try{
            re = std::stoi(res);
        }
        catch(std::exception &e){
            std::string err(e.what());
            LOG(ERROR) << "get " << key << "error:" << err;
            return 0;
        }
        return re;
    }
    double get_double(const std::string &key){
        double re = 0;
        std::string res = this->get_str(key);
        if(res.empty()){
            return re;
        }
        try{
            re = std::stof(res);
        }
        catch(std::exception &e){
            std::string err(e.what());
            LOG(ERROR) << "get " << key << "error:" << err;
            return re;
        }
        return re;
    }
private:
    void read(const std::string &conffile, std::string &content){
        FILE *fl = fopen(conffile.c_str(), "rb");
        fseek(fl, 0, SEEK_END);
        int len = ftell(fl);
        LOG(INFO) << "len:" << len;
        fseek(fl, 0, SEEK_SET);
        char *buf = new char[len+1];
        memset(buf, 0, len+1);
        size_t readLen = fread(buf, 1, len, fl);
        if((int)readLen != len){
            delete []buf;
            fclose(fl);
            return;
        }
        content = std::string(buf);
        delete []buf;
        fclose(fl);
    }

private:
    ini_config(){
    }
    std::map<std::string, std::string> map_;
    static boost::shared_ptr<ini_config> self_;
};
#endif









