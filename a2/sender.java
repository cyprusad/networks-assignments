// sender.java

import java.io.*;  
import java.net.*; 
import java.util.*;
import java.util.Scanner; 
import java.util.List;
import java.util.ArrayList;
import java.util.Timer;
   
class sender { 

    // command line arugment populated members
    private static String nEmulatorHost;
    private static int portData;
    private static int portACK;
    private static String inputFileName;

    // window size
    private static final int maxWindowSize = 10;

    // log file names
    private static final String logSeqNumFile = "seqnum.log";
    private static final String logAckFile = "ack.log";

    // goBackN variables
    private static volatile int lastUnAckedSeqNum;
    private static volatile int lastSentSeqNum;
    private static volatile int currentWindowSize;
    private static int totalPackets;
    private static volatile boolean justRecdAck;
    private static volatile boolean finished = false;
    private static final Object lock = new Object();
  

    // parse file contents into a string
    private static String parseFile(String filename) throws IOException {
      StringBuilder text = new StringBuilder();
      String eol = System.getProperty("line.separator");
      Scanner scanner = new Scanner(new FileInputStream(filename));
      try {
        while(scanner.hasNextLine()) {
          text.append(scanner.nextLine() + eol);
        }
      }
      finally {
        scanner.close();
      }
      return text.toString();
    }

    // make packets out of the string read from a file
    private static List<packet> makePackets(String text) throws Exception {
      
      // max data length per packet
      int maxDataLength = packet.getMaxDataLength();
      int i = 0; // iterator
      char[] chars = text.toCharArray();
      List<packet> packets = new ArrayList<packet>();

      for (i = 0; i < text.length(); i=i + maxDataLength) {
        StringBuffer sb = new StringBuffer();

        if(i + maxDataLength < chars.length) {
          sb.append(chars, i, maxDataLength);
        } else {
          sb.append(chars, i, chars.length - i);         
        }

        // create a data packet and add to list of sequentially numbered packets
        packet pk = packet.createPacket((int)(i/maxDataLength), sb.toString());
        packets.add(pk);
      }

      totalPackets = packets.size();

      return packets;
    }

    // log sequence numbers of packets being sent out
    private static void logSeqNum(int seqNum) {
      try {
          PrintWriter out = new PrintWriter(new BufferedWriter(new FileWriter(logSeqNumFile, true)));
          out.println(seqNum);
          out.close();
      } catch (IOException e) {
          // No-op for now
      }
    }

    // log sequence numbers of ACK packets being received
    private static void logAck(int seqNum) {
      try {
          PrintWriter out = new PrintWriter(new BufferedWriter(new FileWriter(logAckFile, true)));
          out.println(seqNum);
          out.close();
      } catch (IOException e) {
          // No-op for now
      }

    }

    private static void sendPacket(String hostname, int port, packet pk) throws Exception{
      DatagramSocket socket = new DatagramSocket(9867);
      byte[] sendData = pk.getUDPdata();

      InetAddress IPAddress = InetAddress.getByName(hostname);

      DatagramPacket sendPacket = new DatagramPacket(sendData, sendData.length, IPAddress, port);
      socket.send(sendPacket);
    }

    private static void startListeningForAcks(String hostname, int port) throws Exception{
      DatagramSocket socket = new DatagramSocket(port);
      byte[] receiveData = new byte[512];

      while(true) {
        DatagramPacket receivePacket = new DatagramPacket(receiveData, receiveData.length);
        socket.receive(receivePacket);
        packet pk = packet.parseUDPdata(receivePacket.getData());
        
        
        synchronized(lock) {
          currentWindowSize--;
          lastUnAckedSeqNum = pk.getSeqNum();
          justRecdAck = true;
        }

        logAck(lastUnAckedSeqNum);

        // no more unacked packets
        if (lastUnAckedSeqNum == totalPackets) {
          synchronized(lock) {
            finished = true;
          }
          break; 
        }
      }

    }

    private static void beginSending(final String hostname, final int ackPort, final int emuPort, List<packet> packets) throws Exception {
      // initial conditions
      lastUnAckedSeqNum = 0;
      lastSentSeqNum = 0;
      currentWindowSize = 0;

      new Thread(new Runnable() {
          public void run() {
              try { 
                sender.startListeningForAcks(hostname, ackPort);
              } catch (Exception e) {
                //NOOP
              }
          }
      }).start();
      
      do {
        startSendingInWindow(hostname, emuPort, packets);
      } while(lastSentSeqNum >= lastUnAckedSeqNum && finished == false);

    }

    private static void startSendingInWindow(String hostname, int port, List<packet> packets) throws Exception{
      long t = System.currentTimeMillis();
      long end = t + 1000; 
      synchronized(lock) {
        lastSentSeqNum = lastUnAckedSeqNum;
      }
      
      while (System.currentTimeMillis() < end && finished == false) {
        // send next packet
        synchronized(lock) {
          if (justRecdAck == true) {
            justRecdAck = false;
            break;
          }
        }
        if (currentWindowSize < maxWindowSize) {
          sendPacket(hostname, port, packets.get(lastSentSeqNum));
          synchronized(lock) {
            currentWindowSize++;
            lastSentSeqNum++;
            logSeqNum(lastSentSeqNum); //logging
          }   
        } else { // window size exceeded
          Thread.sleep(2000);
        }
        
      }
    }

    public static void main(String args[]) throws Exception  { 
      
      // clear seqnum.log and ack.log from previous run
      freshStart();

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

      List<packet> packets = makePackets(parseFile(inputFileName));

      beginSending(nEmulatorHost, portACK, portData, packets);
      
    }

    // clear up log files from previous run
    private static void freshStart() {
      File seqNumFile = new File(logSeqNumFile);
      File ackFile = new File(logAckFile);
      try {
        if (seqNumFile.exists()) {
          seqNumFile.delete();
        }
        if (ackFile.exists()) {
          ackFile.delete();
        }
      } catch (Exception e) {
        //No-op for now
      }
    }

    private static void usage() {
      System.out.println("Usage:");
      System.out.println("------");
      System.out.println("java sender <host address of the network emulator> <UDP port number used by the emulator to receive data from the sender> <UDP port number used by the sender to receive ACKs from the emulator> <name of the file to be transferred>");
    }  
}  
