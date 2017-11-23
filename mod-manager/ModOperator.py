#encoding=utf-8
import time
import sys
import MySQLdb

db = MySQLdb.connect(host='localhost', port = 3306, user='root', passwd='?????????', db ='dnsserver')
cur = db.cursor()

op = sys.argv[1]
modid = sys.argv[2]
cmdid = sys.argv[3]
ip = sys.argv[4]
port = sys.argv[5]

ts = int(time.time())

if op == 'online':#online some node
    cur.execute('INSERT INTO DnsServerRoute(modid, cmdid, serverip, serverport) VALUES(%s, %s, %s, %s)' % (modid, cmdid, ip, port))
    cur.execute('UPDATE RouteVersion SET version = %d WHERE id = 1' % ts)
    cur.execute('INSERT INTO ChangeLog(modid, cmdid, version) VALUES(%s, %s, %d)' % (modid, cmdid, ts))
elif op == 'offline':#offline some node
    cur.execute('DELETE FROM DnsServerRoute WHERE modid = %s and cmdid = %s and serverip = %s and serverport = %s' % (modid, cmdid, ip, port))
    cur.execute('UPDATE RouteVersion SET version = %d WHERE id = 1' % ts)
    cur.execute('INSERT INTO ChangeLog(modid, cmdid, version) VALUES(%s, %s, %d)' % (modid, cmdid, ts))

cur.close()
db.commit()
db.close()
