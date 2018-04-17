#include "log.h"
#include "Route.h"
#include "config_reader.h"
#include "SubscribeList.h"
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include <vector>

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
    mysql_ping(&_dbConn);
    _tmpData->clear();
    //read from DB
    snprintf(_sql, 100, "SELECT * FROM DnsServerRoute;");
    int ret = mysql_real_query(&_dbConn, _sql, strlen(_sql));
    if (ret)
    {
        log_error("Failed to find any records and caused an error: %s\n", mysql_error(&_dbConn));
        return -1;
    }

    MYSQL_RES *result = mysql_store_result(&_dbConn);
    if (!result)
    {
        log_error("Error getting records: %s\n", mysql_error(&_dbConn));
        return -1;
    }

    long lineNum = mysql_num_rows(result);
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
    log_debug("load data size is %lu", _tmpData->size());
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

Route::Route(): routeVersion(0)
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

    mysql_init(&_dbConn);
    mysql_options(&_dbConn, MYSQL_OPT_CONNECT_TIMEOUT, "30");
    //when close down, let mysql_ping auto reconnect
    my_bool reconnect = 1;
    mysql_options(&_dbConn, MYSQL_OPT_RECONNECT, &reconnect);

    if (!mysql_real_connect(&_dbConn, dbHost, dbUser, dbPasswd, dbName, dbPort, NULL, 0))
    {
        log_error("Failed to connect to MySQL[%s:%u %s %s]: %s\n", dbHost, dbPort, dbUser, dbName, mysql_error(&_dbConn));
        ::exit(1);
    }
    //load version
    int ret = loadVersion();
    if (ret == -1)
    {
        ::exit(1);
    }

    //build _data and _tmpData
    snprintf(_sql, 1000, "SELECT * FROM DnsServerRoute;");
    ret = mysql_real_query(&_dbConn, _sql, strlen(_sql));
    if (ret)
    {
        log_error("Failed to find any records and caused an error: %s\n", mysql_error(&_dbConn));
        ::exit(1);
    }

    MYSQL_RES *result = mysql_store_result(&_dbConn);
    if (!result)
    {
        log_error( "Error getting records: %s\n", mysql_error(&_dbConn));
        ::exit(1);
    }

    long lineNum = mysql_num_rows(result);
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
    log_info("init load data size is %lu", _tmpData->size());
    mysql_free_result(result);
}

int Route::loadVersion()
{
    snprintf(_sql, 1000, "SELECT version from RouteVersion WHERE id = 1;");
    int ret = mysql_real_query(&_dbConn, _sql, strlen(_sql));
    if (ret)
    {
        log_error("Failed to find any records and caused an error: %s\n", mysql_error(&_dbConn));
        return -1;
    }

    MYSQL_RES *result = mysql_store_result(&_dbConn);
    if (!result)
    {
        log_error( "Error getting records: %s\n", mysql_error(&_dbConn));
        return -1;
    }

    long lineNum = mysql_num_rows(result);
    if (lineNum == 0)
    {
        log_error( "No version in table RouteVersion: %s\n", mysql_error(&_dbConn));
        return -1;
    }
    MYSQL_ROW row = mysql_fetch_row(result);
    long newVersion = atol(row[0]);
    if (newVersion == routeVersion)
    {
        //load ok but no change
        return 0;
    }
    routeVersion = newVersion;
    log_info("new route version is %ld", routeVersion);
    mysql_free_result(result);
    return 1;
}

void Route::loadChanges(std::vector<uint64_t>& changes)
{
    snprintf(_sql, 1000, "SELECT modid,cmdid FROM ChangeLog WHERE version <= %ld;", routeVersion);
    int ret = mysql_real_query(&_dbConn, _sql, strlen(_sql));
    if (ret)
    {
        log_error("Failed to find any records and caused an error: %s\n", mysql_error(&_dbConn));
        return ;
    }

    MYSQL_RES *result = mysql_store_result(&_dbConn);
    if (!result)
    {
        log_error( "Error getting records: %s\n", mysql_error(&_dbConn));
        return ;
    }

    long lineNum = mysql_num_rows(result);
    if (lineNum == 0)
    {
        log_error( "No version in table ChangeLog: %s\n", mysql_error(&_dbConn));
        return ;
    }
    MYSQL_ROW row;
    for (long i = 0;i < lineNum; ++i)
    {
        row = mysql_fetch_row(result);
        int modid = atoi(row[0]);
        int cmdid = atoi(row[1]);
        uint64_t key = (((uint64_t)modid) << 32) + cmdid;
        changes.push_back(key);
    }
    mysql_free_result(result);
}

void Route::rmChanges(bool recent)
{
    if (recent)
    {
        snprintf(_sql, 1000, "DELETE FROM ChangeLog WHERE version <= %ld;", routeVersion);
    }
    else
    {
        snprintf(_sql, 1000, "DELETE FROM ChangeLog;");
    }
    int ret = mysql_real_query(&_dbConn, _sql, strlen(_sql));
    if (ret)
    {
        log_error("Failed to delete any records and caused an error: %s\n", mysql_error(&_dbConn));
        return ;
    }
}

//backend thread domain
void* dataLoader(void* args)
{
    int ws = config_reader::ins()->GetNumber("mysql", "load_interval", 10);
    long lstLoadTs = ::time(NULL);
    //firstly, remove all the changes in change log
    Singleton<Route>::ins()->rmChanges(false);
    while (true)
    {
        ::sleep(1);
        long currTs = ::time(NULL);
        //check Route Version
        int ret = Singleton<Route>::ins()->loadVersion();
        if (ret == 1)
        {
            //has change in route
            //reload route data from Mysql to tmpdata
            if (Singleton<Route>::ins()->reload() == 0)
            {
                //load success, then swap data and tmpdata
                Singleton<Route>::ins()->swap();
                lstLoadTs = currTs;
                //then, get changes
                std::vector<uint64_t> changes;
                Singleton<Route>::ins()->loadChanges(changes);
                //push changes to agent
                Singleton<SubscribeList>::ins()->push(changes);
                //then, remove these changes in change log
                Singleton<Route>::ins()->rmChanges();
            }
        }
        else
        {
            if (currTs - lstLoadTs >= ws)
            {
                //reload route data from Mysql to tmpdata
                if (Singleton<Route>::ins()->reload() == 0)
                {
                    //load success, then swap data and tmpdata
                    Singleton<Route>::ins()->swap();
                    lstLoadTs = currTs;
                }
            }
        }
    }
    return NULL;
}
