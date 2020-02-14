#ifndef _YPGH_
#define _YPGH_
#include "base.h"
extern "C"{
    #include <libpq-fe.h>
    #include <libpq/libpq-fs.h>
}




typedef std::map<std::string, std::string> pglinen;
typedef boost::shared_ptr<pglinen> pgline;


const static char* conn_keys[]={
		"user",
		"password",
		//"hostaddr",
        "host",
		"port",
		"dbname",
		"connect_timeout",
		"application_name",
		NULL
};

struct pg_col{
    int type_;
    bool likestr_;
    std::string name_;
    pg_col(){
        likestr_ = true;
        type_ = 0;
    }
};

struct pg_error_mgr{
private:
    static boost::shared_ptr<pg_error_mgr> self_;
    std::map<std::string, std::string> map_;
    static boost::mutex mu_;
    pg_error_mgr(){
    }
public:
    static boost::shared_ptr<pg_error_mgr> instance(){
        boost::mutex::scoped_lock lock(mu_);
        if(self_.get() == NULL){
            self_.reset(new pg_error_mgr());
            self_->init();
        }
        return self_;
    }
    bool is_in(const std::string &code){
        std::map<std::string, std::string>::iterator it =
            map_.find(code);
        return it!=map_.end();
    }

private:
    void init(){
        map_.insert(std::make_pair("42701", "duplicate_column"));
        map_.insert(std::make_pair("42P03", "duplicate_cursor"));
        map_.insert(std::make_pair("42P04", "duplicate_database"));
        map_.insert(std::make_pair("42723", "duplicate_function"));
        map_.insert(std::make_pair("42P05", "duplicate_prepared_statement"));
        map_.insert(std::make_pair("42P06", "duplicate_schema"));
        map_.insert(std::make_pair("42P07", "duplicate_table"));
        map_.insert(std::make_pair("42712", "duplicate_alias"));
        map_.insert(std::make_pair("42710", "duplicate_object"));
        //exe_core sql(sync_insert into testa (id, num) values (3, 1111);) error,, msg:[ERROR:  23505: duplicate key value violates unique constraint "testa_pkey"
        /*have a think about this situation:
        nmsg_ {
          vct_ {
            key_: "redis_china_127.0.0.1:4000_583"
            a_: 0
            b_: 0
            posm_: 2
            poss_: 191
          }
          cmd_: "setddl"
          args_: "china_127.0.0.1:4000_583"
          args_: "create table testa (id int, name text, num int, primary key(id));comment on table testa is \'global_sync=${num}\';insert into testa values (1, \'xyz\', 1111),(2, \'xyz\', 1111),(3, \'xyz\', 1111);"
        } 
        */ 
        map_.insert(std::make_pair("23505", "unique_violation")); 
        // logic replication advance function 
        map_.insert(std::make_pair("55000", "cannot advance replication slot to"));
        // 2019.03.20 for fix reconsume the ddl:[ALTER TABLE ONLY public.system_config ADD CONSTRAINT pk_system_config_item_id_0 PRIMARY KEY (id)]
        map_.insert(std::make_pair("42611", "invalid_column_definition"));
        map_.insert(std::make_pair("42P11", "invalid_cursor_definition"));
        map_.insert(std::make_pair("42P12", "invalid_database_definition"));
        map_.insert(std::make_pair("42P13", "invalid_function_definition"));
        map_.insert(std::make_pair("42P14", "invalid_prepared_statement_definition"));
        map_.insert(std::make_pair("42P15", "invalid_schema_definition"));
        map_.insert(std::make_pair("42P16", "invalid_table_definition"));//multiple primary keys for table:[ALTER TABLE ONLY public.system_config ADD CONSTRAINT pk_system_config_item_id_0 PRIMARY KEY (id)]
        map_.insert(std::make_pair("42P17", "invalid_object_definition"));
    }
};
boost::shared_ptr<pg_error_mgr> pg_error_mgr::self_;
boost::mutex pg_error_mgr::mu_;

class ypg_n{
public:
    ~ypg_n(){
		if(conn_ != NULL){
			PQfinish(conn_);
			conn_ = NULL;
		}
    }

private:
    PGconn *conn_;
    std::string user_;
    std::string pwd_;
    std::string ip_;
    std::string ports_;
    std::string dbname_;
    
    PGresult *exe_core(const std::string &sql, bool only_exec){
        PGresult *result = NULL;
        while(1){
            result = PQexec(conn_, sql.c_str());
            if(result != NULL){
                if(PQresultStatus(result) != PGRES_FATAL_ERROR){	
                    break;
                }
                char *msg0 = PQresultVerboseErrorMessage(result, PQERRORS_VERBOSE, PQSHOW_CONTEXT_ALWAYS);
                std::string msg;
                if(msg0){
                    msg = std::string(msg0);
                    PQfreemem(msg0);
                }
                char *code = PQresultErrorField(result, PG_DIAG_SQLSTATE);
                char *st = PQcmdStatus(result); 
                LOG(WARNING) << "exe_core sql(" << sql << ") error,"
                    << ", msg:[" << msg << "]"
                    << ", code:[" << code << "]"
                    << ",st:[" << st << "]";

                if(only_exec){
                    // situation----
                    // https://www.postgresql.org/docs/11/errcodes-appendix.html
                    // 1. permission
                    // 2. table not exist
                    // 3. sql gram error
                    // 4. disk full
                    // 5. duplicate key--- will no reconsume, we return true in pg
                    if(!pg_error_mgr::instance()->is_in(code)){
                        LOG(WARNING) << "RRRR: not in pg_error_mgr, will reconsume";
                        PQclear(result);
                        result = NULL;
                    }
                    else{
                        LOG(WARNING) << "NNNN: is in pg_error_mgr, will not reconsume";
                    }
                }
                else{
                    PQclear(result);
                    result = NULL;
                }
            }
            ConnStatusType statu = PQstatus(conn_);
            if(CONNECTION_OK == statu){
                    LOG(WARNING) << "result is null, and connection not broken, but failed.";
                    break;
            }
            LOG(ERROR) << "connection is broken, reconnect";
            bool re = false;
            for(int i = 0; i < 3; i++){
                re = connect();
                if(!re){
                    LOG(WARNING) << "reconnect error, we sleep 200ms, and then continue";
                    usleep(200*1000);
                    continue;
                }
                break;
            }
            if(!re){
                conn_ok_ = false;
                return NULL;
            } 
        }
        return result;
    }
public:
    bool conn_ok_; 
    ypg_n(const std::string &user,
        const std::string &pwd,
        const std::string &ip,
        const std::string &ports,
        const std::string &dbname){
        user_ = user;
        pwd_ = pwd;
        ip_ = ip;
        ports_ = ports;
        dbname_ = dbname;
        conn_ = NULL;
        conn_ok_ = false;
    }
    bool connect(){
		if(conn_ != NULL){
			PQfinish(conn_);
			conn_ = NULL;
		}

		const char * const params[]={
			user_.c_str(),			
			pwd_.c_str(),
			ip_.c_str(),
			ports_.c_str(),
			dbname_.c_str(),
			"10",
			"yrep",
			NULL
		};
		conn_ = PQconnectdbParams(conn_keys, params, 0);
        if(PQstatus(conn_) != CONNECTION_OK){
			LOG(ERROR) << "connect to pg error"; 
            return false;
        }
        conn_ok_ = true;
        return true;
    }
    int exec(const std::string &sql){
        PGresult *result = exe_core(sql, true);
        if(result == NULL){
            return -1;
        }
        int num = 0;
        char *uprows = PQcmdTuples(result);
        if(uprows == NULL){
            PQclear(result);
            return num;
        }
        if(strlen(uprows)==0){
            PQclear(result);
            return num;
        } 
        try{
            num = std::stoi(uprows);
        }
        catch(const std::exception &e){
            LOG(ERROR) << "error parse number string:[" << uprows << "]";
        }
        PQclear(result);
        return num;
    }
 	bool query(const std::string &sql,
				std::vector<pgline> &lines) {
        PGresult *result = exe_core(sql, false);
        if(result == NULL){
            return false;
        }

        int rows = PQntuples(result);
        int cols = PQnfields(result);

        std::map<int, pg_col> col_map;
        for(int col = 0; col < cols; col++){
            char *col_name = PQfname(result, col);
            int type = PQftype(result, col);
            pg_col pgcol;
            pgcol.type_ = type;
            pgcol.name_ = std::string(col_name);
            pgcol.likestr_ = true;
            if(type==20 ||
                type==21 ||
                type==23 ||
                type==700 ||
                type==701){
                pgcol.likestr_ = false;
            }
            col_map.insert(std::make_pair(col, pgcol));
        }

        for(int row = 0; row < rows; row++){
            pgline line(new pglinen());
            for(int col = 0; col < cols; col++){
                char *tmp = PQgetvalue(result, row, col);
                int len = PQgetlength(result, row, col);
                
                std::string value = "";
                pg_col pgcol = col_map[col];
                if(pgcol.type_ != 17){
                    value = std::string(tmp);
                }
                else{
                    size_t buflen = 0;
                    unsigned char * buf = PQunescapeBytea((const unsigned char *)tmp, &buflen);
                    value = std::string((char*)buf, buflen);
                    PQfreemem(buf);
                    #if 0
                    yrepn::yrep_line yl;
                    try{
                        yl.ParseFromString(value);
                    }
                    catch(const std::exception &e){
                        LOG(ERROR) << "error to parse value to yrep_line:" << e.what();
                        continue;
                    }
                    value = yl.DebugString();
                    #endif
                }
                //LOG(ERROR) << col_map[col].name_ << ", len:" << len << ", value:" << value;
                line->insert(
                        std::make_pair(pgcol.name_, value)
                    );
            }
            lines.push_back(line);
        }
        PQclear(result);
        return true;
    }
    uint32_t new_lo(unsigned char *buf, int len){
        uint32_t id = lo_creat(conn_, INV_READ|INV_WRITE);
        int fd = lo_open(conn_, id, INV_WRITE);
        int wlen = lo_write(conn_, fd, (const char*)buf, (size_t)len);
        LOG(ERROR) << "fd:" << fd << ", wlen:" << wlen << ", id:" << id;
        lo_close(conn_, fd);
        return id;
    }
    bool read_lo(uint32_t id, char **buf, uint64_t *len){
        int fd = lo_open(conn_, id, INV_READ);
        if(fd < 0){
            return false;
        }
        int rr = lo_lseek(conn_, fd, 0, SEEK_END); 
        if(rr < 0){
            LOG(ERROR) << "rr:[" << rr << "], err:" << PQerrorMessage(conn_);
            return false;
        }
        uint64_t len0 = lo_tell64(conn_, fd);
        *len = len0;

        *buf = new char[len0];
        memset(*buf, 0, *len);
        lo_lseek(conn_, fd, 0, SEEK_SET);
        int rlen = lo_read(conn_, fd, *buf, *len); 
        lo_close(conn_, fd);
        return rlen==len0;
    }
    bool rm_lo(uint32_t id){
        int re = lo_unlink(conn_, id);
        return re==1;
    }
    int ydec_read(char **buf){
        std::string begin_lsn = "NULL";
        PGresult *res = NULL;
        int rlen = pq_ydec_read(conn_, "yrep", 2000, (char*)begin_lsn.c_str(), buf, &res);
        if (PQresultStatus(res) != PGRES_COMMAND_OK){
            char *msg0 = PQresultVerboseErrorMessage(res, PQERRORS_VERBOSE, PQSHOW_CONTEXT_ALWAYS);
            LOG(ERROR) << "msg0:" << msg0;
            PQfreemem(msg0);
        }
        PQclear(res);
        return rlen;
    }

};
typedef boost::shared_ptr<ypg_n> ypg;

#endif





