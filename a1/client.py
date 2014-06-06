#!/usr/bin/python
import sys
import socket

## Usage message triggered when incorrect or wrong number of arguments called with the script
def usage():
    print 'Incorrect number of arguments specified'
    print 'Usage: python client.py <server_address>, <n_port>, <msg>'
  
## Main execution of the client program
def main(argv):
  if len(argv) == 3:
    server_address, n_port, message = argv
  else: 
    usage()
    return # could be moved to try/catch block?
  
  ## Stage 1
  r_port = negotiate(server_address, n_port)

  ## Stage 2
  transact(server_address, r_port, message)

'''
  Negotiation phase of the signalling
'''
def negotiate(server_address, n_port):
  sock = socket.socket(socket.AF_INET, socket.SOCK_STREAM)
  sock.connect(('', int(n_port)))
  sock.send('13') ## Initiate TCP connection
  buf = sock.recv(1024)
  if len(buf) > 0:
    sock.close()
    return buf

'''
  Transaction phase of the signalling
'''
def transact(server_address, r_port, message):
  ## Send message
  sock = socket.socket(socket.AF_INET, socket.SOCK_DGRAM)
  sock.sendto(message, ('', int(r_port)))

  ## Receive reversed message
  buf, address = sock.recvfrom(1024)
  if len(buf) > 0:
    print buf
    sock.close()


if __name__ == '__main__':
  main(sys.argv[1:])