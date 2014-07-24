// This server is supposed to receive a sentence from the client, capitalizes it and sends it back to the client.

import java.io.*;  
import java.net.*;  
   
class receiver { 

  // command line arugment populated members
  private static String nEmulatorHost;
  private static int portData;
  private static int portACK;
  private static String inputFileName;

  // state variables
  private static int lastArrived = -1;
  private static int lastAcked;

  // log file name
  private static final String logArrivalFile = "arrival.log";

  // log sequence numbers of all packets received
  private static void logArrival(int seqNum) {
    try {
        PrintWriter out = new PrintWriter(new BufferedWriter(new FileWriter(logArrivalFile, true)));
        out.println(seqNum);
        out.close();
    } catch (IOException e) {
        // No-op for now
    }

  }

  // clear up log files from previous run
  private static void freshStart() {
    File arrivalFile = new File(logArrivalFile);
    try {
      if (arrivalFile.exists()) {
        arrivalFile.delete();
      }
    } catch (Exception e) {
      //No-op for now
    }
  }

  public static void main(String args[]) throws Exception  
    { 
      //clear arrival log file from prev run 
      freshStart();

      // parse command line args
      if (args.length > 0) {
        try {
          nEmulatorHost = args[0]; 
          portACK = Integer.parseInt(args[1]);
          portData = Integer.parseInt(args[2]);
          inputFileName = args[3];
        } catch (NumberFormatException e) {
          System.err.println("Argument " + args[2] + " and " + args[3] + " must be integers.");
          usage();
          System.exit(1);
        } 
      }

      DatagramSocket serverSocket = new DatagramSocket(portData);  
   
      byte[] receiveData = new byte[1024];  
      byte[] sendData  = new byte[1024];  
      
      while(true)  
        {  
   
          DatagramPacket receivePacket =  
             new DatagramPacket(receiveData, receiveData.length);  
          
          serverSocket.receive(receivePacket);  
 
          packet pk = packet.parseUDPdata(receivePacket.getData());
          int receivedSeqNum = pk.getSeqNum();

          logArrival(receivedSeqNum); //log the packet that arrived

          InetAddress IPAddress = InetAddress.getByName(nEmulatorHost);
          
          int port = portACK;  

          if (receivedSeqNum == lastArrived + 1) {
            lastArrived = receivedSeqNum;
            lastAcked = receivedSeqNum;
            if (pk.getType() == 2) {
              packet ack = packet.createACK(lastAcked);  
              
              sendData = ack.getUDPdata(); 
              
              DatagramPacket sendPacket =  
                 new DatagramPacket(sendData, sendData.length, IPAddress,  
                                   port);  
              
              serverSocket.send(sendPacket);  
              break;
            }
          }
          
          packet ack = packet.createACK(lastAcked);  
 
          sendData = ack.getUDPdata(); 
   
          DatagramPacket sendPacket =  
             new DatagramPacket(sendData, sendData.length, IPAddress,  
                               port);  
   
          serverSocket.send(sendPacket);  
        }

       
    } 

    private static void usage() {
      System.out.println("Usage:");
      System.out.println("------");
      System.out.println("java receiver <hostname for the network emulator> <UDP port number used by the link emulator to receive ACKs from the receiver> <UDP port number used by the receiver to receive data from the emulator> <name of the file into which the received data is written>");
    }   
}   
