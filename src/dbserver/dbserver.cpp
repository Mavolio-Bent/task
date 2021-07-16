#include <pqxx/pqxx>
#include <mqtt/client.h>
#include <iostream>
#include <boost/algorithm/string.hpp>
#include <boost/algorithm/string/classification.hpp>
#include <boost/property_tree/json_parser.hpp>
#include <sstream>

using namespace std;
using namespace pqxx; 
const string DB_NAME {"devices"};
const string HELP_MESSAGE {"Usage: dbserver <dbuser> <password> <address> <port> <mqtt-broker address>\n"};
string mqtt_host;

class SelectSQL {
    private:
    std::string tables;
    std::string columns;
    std::string whereClauses;
    public: 
    SelectSQL(std::string arg) {
        //first separate tables from queries
        vector<string> separated;
        boost::split(separated, arg, boost::is_any_of("/"));
        //res is guaranteed to have at least 2 elements
        tables = separated[1];
        //in case we got something like /table/foo (foo may be empty)
        if (separated.size() == 2) {
            columns = "*";
        }
        else if (separated[2] == "") {
            columns = "*";
        }
        else {
            vector<string> queriesAndClauses;    
            boost::split(queriesAndClauses, separated[2], boost::is_any_of("?"));
            if (queriesAndClauses[0] == "") {
                columns = "*";
                whereClauses = queriesAndClauses[1];
            }
            else {    
                columns = queriesAndClauses[0];
                if (queriesAndClauses.size() != 1) {
                    whereClauses = queriesAndClauses[1];
                }
            }  
        }          
    }
    std::string selectToSql() {
        string sql = "SELECT " + columns + " FROM " + tables;
        if (!whereClauses.empty()) {
            sql = sql + " WHERE " + whereClauses;
        }
        sql = sql + ";";
        return sql;
    }
};

class DeleteSQL {
    private:
    string table;
    string deleteClause;
    public:
    DeleteSQL(string path) {
        vector<string> separated;
        boost::split(separated, path, boost::is_any_of("/"));
        table = separated[1];
        if (separated.size() == 2) {
            deleteClause = "";
        }
        else {
            deleteClause = separated[2]; 
        }
    }
    string deleteToSQL() {
        string sql = "DELETE FROM " + table;
        if (!deleteClause.empty()) {
            sql = sql + " WHERE " + deleteClause;
        }
        sql = sql + ";";
        return sql;
    }
    
};

class InsertSQL {
    private:
    string table;
    vector<string> columns;
    vector<vector<string>> values;
    public:
    InsertSQL(string target) {
        
    }
};

string format_res(result r) {
    string formatted = "{ ";
    int i = 1;
    for (auto row = r.begin(); row != r.end(); row++, i++) {
        formatted = formatted + "\"row " + to_string(i) + "\": [";
        int k = 0;
        for (auto field = row.begin(); field != row.end(); field++, k++) {
            formatted = formatted + "\"" + field->c_str() + "\"";
            if (k != row.size()-1) {
                formatted = formatted + ",";
            }
        }
        formatted = formatted + "]";
        if (i != r.size()) {
            formatted = formatted +",";
        }
    }
    formatted = formatted + " }";
    return formatted;
}


void select(pqxx::connection& conn, mqtt::client& client, string target) {
    SelectSQL sel(target);
    string sql = sel.selectToSql();   
    string res{""};
    try {
        work w(conn);
        result r = w.exec(sql);
        w.commit();
        res = format_res(r);
        auto pubmsg = mqtt::make_message("out", res);                     
        pubmsg->set_qos(1);
        client.publish(pubmsg); 
    }
    catch (const exception &e) {
        res = e.what();      
        auto pubmsg = mqtt::make_message("err", res);                     
        pubmsg->set_qos(1);
        client.publish(pubmsg);                   
    } 
}

void del(pqxx::connection& conn, mqtt::client& client, string target) {
    DeleteSQL delet(target);
    string sql = delet.deleteToSQL();   
    string res{""};
    try {
        work w(conn);
        result r = w.exec(sql);
        w.commit();
        auto pubmsg = mqtt::make_message("out", "{\"success\": \"true\"}");                     
        pubmsg->set_qos(1);
        client.publish(pubmsg); 
    }
    catch (const exception &e) {
        res = e.what();      
        auto pubmsg = mqtt::make_message("err", "{\"success\": \"false\"}");                     
        pubmsg->set_qos(1);
        client.publish(pubmsg);                   
    } 
}

int main(int argc, char* args[]) {
    try {
        if (argc != 6) {
            cerr << HELP_MESSAGE;
            return 1;
        }
        
        const string USER = args[1];
        const string PASSWORD = args[2];
        const string ADDRESS = args[3];
        const string PORT = args[4];
        mqtt_host = args[5];
        
        connection conn("dbname = " + DB_NAME + " user = " + USER + 
            " password = " + PASSWORD + " hostaddr = " + ADDRESS + " port = " + PORT);
            
        mqtt::client client(mqtt_host, "dbserver");
        mqtt::connect_options connOps;
        connOps.set_keep_alive_interval(30);
        connOps.set_clean_session(true);
        client.connect(connOps);    
        client.subscribe({"get", "post", "delete"}, {1, 1, 1});
        for (;;) {
            auto msg = client.consume_message();
            if (msg) {
                if (msg->get_topic() == "get") {
                    select(conn, client, msg->to_string());
                }
                else if (msg->get_topic() == "post") {
                }
                else if (msg->get_topic() == "delete") {
                    del(conn, client, msg->to_string());
                }
            }
        }
    } catch (const exception &e) {
      cerr << e.what() << "\n";
      return 1;
    }
}