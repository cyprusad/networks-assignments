#!/usr/bin/python
import sys

## Usage message triggered when incorrect or wrong number of arguments called with the script
def usage():
    print 'Incorrect number of arguments specified'
    print 'Usage: python client.py <server_address>, <n_port>, <msg>'
  
## Main execution of the client program
def main(argv):
  if len(argv) == 3:
    server_address, n_port, msg = argv
  else: 
    usage()
    return # could be moved to try/catch block?
  print 'The client was started and is now connecting with server at:', server_address, 'on port:', n_port, 'with message:', msg 
  ## Actual client code begins here
  

if __name__ == '__main__':
  main(sys.argv[1:])