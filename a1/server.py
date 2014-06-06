#!/usr/bin/python
import socket

## Main execution of the server program
def main():
  n_sock = open_socket('tcp')
  print 'SERVER_PORT =', n_sock.getsockname()[1]

  ## Stage 1
  negotiate(n_sock) ## never close this socket


'''
  Open a port and return the integer port number
  Args: 
    protocol - 'tcp' for negotiation 
               'udp' for transaction
'''
def open_socket(protocol):
  if protocol == 'tcp':
    sock = socket.socket(socket.AF_INET, socket.SOCK_STREAM) ## TCP connection
  else:
    sock = socket.socket(socket.AF_INET, socket.SOCK_DGRAM) ## UDP connection
  sock.bind(('', 0))
  return sock


'''
  Negotiation phase of the signalling
'''
def negotiate(sock):
  sock.listen(1) ## Listen to only one connection at a time

  while True:
    connnection, address = sock.accept()
    buf = connnection.recv(1024) ## Dummy message
    if len(buf) > 0:
      r_sock = open_socket('udp')
      connnection.send(str(r_sock.getsockname()[1]))
      
      ## Stage 2
      transact(r_sock)

'''
  Transaction phase of the signalling
'''
def transact(sock):
  while True:
    buf, address = sock.recvfrom(1024) ## Message
    if len(buf) > 0:
      sock = socket.socket(socket.AF_INET, socket.SOCK_DGRAM)
      sock.sendto(buf[::-1], address)
      sock.close()
      break


if __name__ == '__main__':
  main()