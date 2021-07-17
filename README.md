# Task

## Dependencies:

Ensure that you have following dependencies installed:
`Paho MQTT C++`
`lipqxx`
`Boost C++`

## Build

```
$ git clone https://github.com/Mavolio-Bent/task.git
$ cd task
$ make ARGS=BOOST_ROOT && make install
```
where `BOOST_ROOT` is installation path of Boost. Binaries are then located in `/out` directory.

## Launch
To launch server run
```
$ cd task/out
$ server <host> <port> <mqtt-broker address>
```
To launch dbservice run
```
$ cd task/out
$ dbserver <dbuser> <password> <address> <port> <mqtt-broker address>
```
Ensure that MQTT Broker is active

## Usage
Assuming server is hosted at `address` and there is database named `dbname` on the PostgresSQL server, to select data from table or multiple tables one performs following `GET` HTTP request:
```
$ curl address/dbname/<tables>/<columns>?<clauses>
```
where `<tables>` is list of tables separated by commas without spaces, `<columns>` is list of columns separated by commas without spaces and `?<clauses>` is list of `WHERE` clauses separated by commas without spaces. **Attention: clauses like `foo AND bar`** are not supported.

### Examples:
```
$ curl 127.0.0.1:8080/dvdrental/staff,store/
```
```
$ curl 127.0.0.1:8080/dvdrental/staff,store/?staff_id=manager_staff_id
```
```
$ curl 127.0.0.1:8080/dvdrental/staff/staff_id
```

To delete queries from table perform 
```
$ curl -X "DELETE" address/dbname/table/<clauses>
```
where `<clauses>` is list of clauses for deletion (if list is empty it deletes everything in table).

To insert queries into table perform 
``` 
$ curl -X "POST" -d "@path_to_file" address/dbname/table
```
where `path_to_file` locates json file containing insertion.
Equivalent is
```
$ curl -X "POST" -d JSON_STRING  -H "Content-Type: application-json" address/dbname/table
```
**Attention: this method is not recommended since it requires proper enclosing of columns by backslashes. It is better to send json as file**

Json is assumed to have the contents of following format
```
{
    "columns": ["col_1", ..., "col_N"],
    "row_1": ["'val_1'", ..., "'val_N'"],
    ...,
    "row_N": ["'val_1'", ..., "'val_N'"]
}
```
and is translated to
```
INSERT INTO table (col_1, ..., col_N) VALUES
    ('val_1', ..., 'val_N'),
    ...,
    ('val_1', ..., 'val_N');
```
If no JSON object named `columns` is met `(col_1, ..., col_N)` is assumed to be empty;    



