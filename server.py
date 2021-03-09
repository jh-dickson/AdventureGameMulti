import socket
import os
from _thread import *
import json

#ServerSideSocket.close()

ServerSideSocket =socket.socket()

host ='127.0.0.1'
port=42069
ThreadCount = 0

players = []

try:
	ServerSideSocket.bind((host,port))
except socket.error as e:
	print(e)

print("Socket is running")
ServerSideSocket.listen(3)

def client_connect(con):
	#con.send(str.encode("Connected to server"))
	while True:
		data =con.recv(2048)
		if not data:
			continue
		print("Received: ", data.decode("utf-8"))
		#con.sendall(str.encode("Message Received"))
		json_string = data.decode("utf-8")
		print("JSON STRING:", json_string)
		gameDataObj = json.loads(json_string)
		clientPlayerID = gameDataObj["playerID"]
		print("ClientID: ", clientPlayerID)
		players.append(gameDataObj)	#Add to players list
		print("players list: ", players)
		
		for i in players:
			#print(i["playerID"])
			if (i["playerID"] != clientPlayerID):
				con.sendall(str.encode(json.dumps(i)))
		#print("Player Data Over")
		#con.sendall(str.encode("Player Data Over"))
		#break
	con.close()
		

while True:
	Client, address = ServerSideSocket.accept()
	print("Connected To: ", address[0], ":", address[1])
	start_new_thread(client_connect, (Client, ))
	ThreadCount += 1
	print("Thread count: ", ThreadCount)

ServerSideSocket.close()

#JSON example
# {
#	"playerID" :69,
# 	"positionX" : 80,
#   "positionY" : 30,
#   "Health" : 10,
# 	"Hunger" : 0,
# 	"Swords" : 0,
# 	"Stars" : 0,
# 	"difficulty" : 140,
# 	"seed" : 1000
# }