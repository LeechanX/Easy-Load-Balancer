#include "Route.h"
#include "config_reader.h"
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>

//main threads call it
hostSet Route::getHosts(int modid, int cmdid)
{
    hostSet hosts;
    uint64_t key = ((uint64_t)modid << 32) + cmdid;

    ::pthread_rwlock_rdlock(&_rwlock);
    routeMapIt it = _data->find(key);
    if (it != _data->end())
    {
        hosts = it->second;
    }
    ::pthread_rwlock_unlock(&_rwlock);
    return hosts;
}

//backend thread call it
int Route::reload()
{
    _tmpData->clear();
    //read from DB
    int ret = mysql_real_query(&_dbConn, _sql, strlen(_sql));
    if (ret)
    {
        printf("Failed to find any records and caused an error: %s\n", mysql_error(&_dbConn));
        return -1;
    }

    MYSQL_RES *result = mysql_store_result(&_dbConn);
    if (!result)
    {
        printf( "Error getting records: %s\n", mysql_error(&_dbConn));
        return -1;
    }

    long lineNum = mysql_num_fields(result);
    MYSQL_ROW row;
    for (long i = 0;i < lineNum; ++i)
    {
        row = mysql_fetch_row(result);
        int modid = atoi(row[1]);
        int cmdid = atoi(row[2]);
        //big endian
        unsigned ip = atoi(row[3]);
        int port = atoi(row[4]);

        uint64_t key = ((uint64_t)modid << 32) + cmdid;
        uint64_t value = ((uint64_t)ip << 32) + port;

        (*_tmpData)[key].insert(value);
    }
    mysql_free_result(result);
    return 0;
}

//backend thread call it
void Route::swap()
{
    //swap data ptr
    ::pthread_rwlock_wrlock(&_rwlock);
    routeMap* t = _data;
    _data = _tmpData;
    _tmpData = t;
    ::pthread_rwlock_unlock(&_rwlock);
}

Route::Route()
{
    ::pthread_rwlock_init(&_rwlock, NULL);
    _tmpData = new routeMap();
    _data = new routeMap();

    //connection DBconn
    const char* dbHost   = config_reader::ins()->GetString("mysql", "db_host", "127.0.0.1").c_str();
    uint16_t dbPort      = config_reader::ins()->GetNumber("mysql", "db_port", 3306);
    const char* dbUser   = config_reader::ins()->GetString("mysql", "db_user", "").c_str();
    const char* dbPasswd = config_reader::ins()->GetString("mysql", "db_passwd", "").c_str();
    const char* dbName   = config_reader::ins()->GetString("mysql", "db_name", "dnsserver").c_str();

    mysql_options(&_dbConn, MYSQL_OPT_CONNECT_TIMEOUT, "30");
    if (!mysql_real_connect(&_dbConn, dbHost, dbUser, dbPasswd, dbName, dbPort, NULL, 0))
    {
        printf("Failed to connect to MySQL: Error: %s\n", mysql_error(&_dbConn));
        ::exit(1);
    }

    //build _data and _tmpData
    snprintf(_sql, 100, "SELECT * FROM DnsServerRoute;");
    int ret = mysql_real_query(&_dbConn, _sql, strlen(_sql));
    if (ret)
    {
        printf("Failed to find any records and caused an error: %s\n", mysql_error(&_dbConn));
        ::exit(1);
    }

    MYSQL_RES *result = mysql_store_result(&_dbConn);
    if (!result)
    {
        printf( "Error getting records: %s\n", mysql_error(&_dbConn));
        ::exit(1);
    }

    long lineNum = mysql_num_fields(result);
    MYSQL_ROW row;
    for (long i = 0;i < lineNum; ++i)
    {
        row = mysql_fetch_row(result);
        int modid = atoi(row[1]);
        int cmdid = atoi(row[2]);
        //big endian
        unsigned ip = atoi(row[3]);
        int port = atoi(row[4]);

        uint64_t key = ((uint64_t)modid << 32) + cmdid;
        uint64_t value = ((uint64_t)ip << 32) + port;

        (*_data)[key].insert(value);
    }
    mysql_free_result(result);
}

//backend thread domain
void* dataLoader(void* args)
{
    int ws = config_reader::ins()->GetNumber("mysql", "load_interval", 10);
    while (true)
    {
        ::sleep(ws);
        //reload route data from Mysql to tmpdata
        int ret = Singleton<Route>::ins()->reload();
        if (ret == 0)
        {
            //load success, then swap data and tmpdata
            Singleton<Route>::ins()->swap();
        }
    }
    return NULL;
}
