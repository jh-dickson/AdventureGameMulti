import socket
import json

ClientMultiSocket = socket.socket()
host = '127.0.0.1'
port = 42069

json_string = u"""{
	"playerID" :69,
	"positionX" : 80,
  	"positionY" : 30,
	"inventory" : {
		"Health" : 10,
		"Hunger" : 0,
		"Swords" : 0,
		"Stars" : 0
	},
	"difficulty" : 140,
	"seed" : 1000
}
"""


#gameDataObj = json.loads(json_string)
#print(gameDataObj)

print('Waiting for connection response')
try:
    ClientMultiSocket.connect((host, port))
except socket.error as e:
    print(str(e))

res = ClientMultiSocket.recv(1024)

ClientMultiSocket.send(str.encode(json_string))
while True:
	res = ClientMultiSocket.recv(1024)
	if not res:
		break
	print(res.decode('utf-8'))

ClientMultiSocket.close()