README

Networked Spell Checker
The purpose of this project was to demonstrate how to implement mutual exclusion through mutexes and conditionals when serving multiple
clients from a server.

The project consists of 5 distinct parts:
Reading in a dictionary
Setting up the socket connection (Already provided)
Worker procedure
Logger procedure
Spellchecker (had problems with this)


Starting the server
server : uses default dict and port (words.txt and 8888)
server [dictname or port #]: Uses specified variable and other uses default
server [port#] [dictname]: Specify both port# and dictname. Needs to be in this order

Client
Connect to the server (telnet [ip] [port])
Send words (looped)
Esc to quit the server