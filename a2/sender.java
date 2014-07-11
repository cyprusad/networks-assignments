// This client is supposed to send a sentence to the server, and then print the sentence received by the server. 

import java.io.*;  
import java.net.*;  
   
class sender { 

    // command line arugment populated members
    private static String nEmulatorHost;
    private static int portData;
    private static int portACK;
    private static String inputFileName;


    public static void main(String args[]) throws Exception  {  
      // parse command line args
      if (args.length > 0) {
        try {
          nEmulatorHost = args[0]; 
          portData = Integer.parseInt(args[1]);
          portACK = Integer.parseInt(args[2]);
          inputFileName = args[3];
        } catch (NumberFormatException e) {
          System.err.println("Argument " + args[2] + " and " + args[3] + " must be integers.");
          usage();
          System.exit(1);
        } 
      }


      BufferedReader inFromUser =  
        new BufferedReader(new InputStreamReader(System.in));  
   
      DatagramSocket clientSocket = new DatagramSocket();  
   
      InetAddress IPAddress = InetAddress.getByName("localhost");  //Name of the machine hosting the server
   
      byte[] sendData = new byte[1024];  
      byte[] receiveData = new byte[1024];  
      
      System.out.print("Sentence to Send from the client: ");  
      
      String sentence = inFromUser.readLine();  
      sendData = sentence.getBytes();          
      DatagramPacket sendPacket =  
         new DatagramPacket(sendData, sendData.length, IPAddress, 9876);  
   
      clientSocket.send(sendPacket);  
   
      DatagramPacket receivePacket =  
         new DatagramPacket(receiveData, receiveData.length);  
   
      clientSocket.receive(receivePacket);  
   
      String modifiedSentence =  
          new String(receivePacket.getData());  
   
      System.out.println("FROM SERVER:" + modifiedSentence);  
      clientSocket.close();  
    }

    private static void usage() {
      System.out.println("Usage:");
      System.out.println("------");
      System.out.println("java sender <host address of the network emulator> <UDP port number used by the emulator to receive data from the sender> <UDP port number used by the sender to receive ACKs from the emulator> <name of the file to be transferred>");
    }  
}  
