from multiprocessing import Process
import os
import time
import sys
import socket
import argparse
import random


def ranstr(num):
    # 猜猜变量名为啥叫 H
    H = 'ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789'

    salt = ''
    for i in range(num):
        salt += random.choice(H)

    return salt

def test_client(host, port):

    # 创建TCP套接字
    sock = socket.socket(socket.AF_INET, socket.SOCK_STREAM)

    # 连接服务器
    srv_addr = (host, port)
    sock.connect(srv_addr)

    # 发送并接收数据
    try:
        # 发送消息
        while True:
            msg = ranstr(100)

            sock.sendall(bytes(msg, encoding = "utf8"))

            # 接收消息
            data = sock.recv(1024)
            print ("Message from server: %s" % data)
        sock.close()
    except socket.errno as e:
        print ("Socket error: %s" % str(e))
    except Exception as e:
        print ("Other exception: %s" % str(e))
    finally:
        sock.close()

if __name__=='__main__':
    print ('Parent process %s.' % os.getpid())
    for i in list(range(20)):
        p = Process(target=test_client, args=('9.134.75.173', 8000))
        p.start()
        #p.join()

    print ('Process will start.')

    print ('Process end.')
