// This client is supposed to send a sentence to the server, and then print the sentence received by the server. 

import java.io.*;  
import java.net.*;  
   
class sender {  
    public static void main(String args[]) throws Exception  
    {  
   
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
}  
