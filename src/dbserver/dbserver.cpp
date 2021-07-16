#include <pqxx/pqxx>
#include <mqtt/client.h>
#include <iostream>
#include <boost/algorithm/string.hpp>
#include <boost/algorithm/string/classification.hpp>
using namespace std;
using namespace pqxx; 
const string DB_NAME {"devices"};
const string HELP_MESSAGE {"Usage: dbserver <dbuser> <password> <address> <port> <mqtt-broker address>\n"};
string mqtt_host;


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
vector<string> parse_req(string path) {
    //returns vector of the form 
    // a[1] = table name a[i] are select queries for i > 1
    vector<string> res;
    boost::split(res, path, boost::is_any_of("/,"));
    if (res.back() == "") {
        res.pop_back();
    }
    return res;
}

string handle_select(vector<string> target) {
    string sql = "SELECT ";
    string table = target[1];
    if (target.size() == 2) {
        sql = sql + " * FROM " + table;
    }
    else {
        sql = sql + target[2];
        for (int i = 3; i < target.size(); i++) {
            sql = sql + ", " + target[i];
            }
        sql = sql + " FROM " + table;
    }
    return sql;
}

void select(pqxx::connection& conn, mqtt::client& client, string targ) {
    vector<string> target = parse_req(targ);
    string sql = handle_select(target);       
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
                else if (msg->get_topic() == "post") {}
                else if (msg->get_topic() == "delete") {}
            }
        }
    } catch (const exception &e) {
      cerr << e.what() << "\n";
      return 1;
    }
}