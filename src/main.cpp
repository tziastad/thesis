// Test of cheap 13.56 Mhz RFID-RC522 module from eBay
// This code is based on Martin Olejar's and Thomas Kirchner's MFRC522 libraries. Minimal changes
// Adapted for FRDM-K64F from Freescale, in 07/21/2014 by Clovis Fritzen.

#include "mbed.h"
#include "MFRC522.h"
#include "EthernetInterface.h"
#include "pk.h"
#include "mbed_config.h"
#include <string.h>
#include <stdio.h>
#include "mbedtls/error.h"
#include "mbedtls/pk.h"
#include "mbedtls/entropy.h"
#include "mbedtls/ctr_drbg.h"


// FRDM-K64F (Freescale) Pin for MFRC522 reset
#define MF_RESET PTD0

// Serial connection to PC for output*/
static BufferedSerial pc(PTC17, PTC16); // serial comm on the FRDM board

// MFRC522    RfChip   (SPI_MOSI, SPI_MISO, SPI_SCK, SPI_CS, MF_RESET);
MFRC522 RfChip(PTD2, PTD3, PTD1, PTE25, PTD0);

EthernetInterface net;
DigitalOut LedGreen(LED2);
DigitalOut LedBlue(LED3);
DigitalOut LedRed(LED1);

struct message
{ // Structure declaration
  char type;
  int length;
  char *payload;
};

void turnOnBlueLight()
{
  LedGreen = 1; // off
  LedBlue = 0;  // on
  LedRed = 1;   // off
}
void turnOnRedLight()
{
  LedGreen = 1; // off
  LedBlue = 1;  // off
  LedRed = 0;   // on
}

void turnOnGreenLight()
{
  LedGreen = 0; // on
  LedBlue = 1;  // off
  LedRed = 1;   // off
}

void turnOffAllLights()
{
  LedGreen = 1; // off
  LedBlue = 1;  // off
  LedRed = 1;   // off
}

void findUniqueDeviceId(char temp[])
{

  // 4 registers with 32 bit- 4 bytes each and all together gives 128 bit unique id of the device- see manual page 305
  int IdentificationRegistersAddresses[] = {0x40048054, 0x40048058, 0x4004805C, 0x40048060};
  int i;
  for (i = 0; i < 4; i++)
  {
    char *pointer = (char *)IdentificationRegistersAddresses[i];

    temp[1 + i * 4] = pointer[0];
    temp[1 + (i * 4) + 1] = pointer[1];
    temp[1 + (i * 4) + 2] = pointer[2];
    temp[1 + (i * 4) + 3] = pointer[3];
  }
  /*
    for (int i = 0; i < 16; ++i) {
        printf("%x", temp[i]);
    }
  printf("\r\n");*/
}

void print_id(char *array, int n)
{
  printf("%.*s ", 1, array);
  for (int i = 1; i < n; i++)
  {
    printf("%X ", array[i]);
  }
  printf("\r\n");
}

void print_array(unsigned char *array, int n)
{

  for (int i = 0; i < n; i++)
  {
    printf("%X ", array[i]);
  }
  printf("\r\n");
}

void print_byte_array(char * array, int n){
   for (int i = 0; i < n; i++)
  {
    printf("%X ", array[i]);
  }
  printf("\r\n");
}

int sendMessageToServer(TCPSocket *socket, struct message msg)
{
  char dev_type[] = "#";
  char card_type[] = "@";
  char hello_msg_type[] = "!";
  // Send the unique id of the RFID card to server
  int sent_bytes = (*socket).send(msg.payload, msg.length);
  printf("sent bytes are: %d, ", sent_bytes);
  if (msg.type == dev_type[0])
  {
    printf("Device UID: ");
    print_id(msg.payload, msg.length);
  }
  else if (msg.type == card_type[0])
  {
    printf("Card UID: ");
    print_id(msg.payload, msg.length);
  }
  else if (msg.type == hello_msg_type[0])
  {
    printf("%s", msg.payload);
    printf("\n\r");
  }
  return sent_bytes;
}

void recieveResponseFromServer(TCPSocket *socket, int bufferLength, int isCard)
{

  char allow[] = "Door is opened.";
  char do_not_allow[] = "Sorry, you can't access.";
  // Recieve a simple response and print out the response line
  char rbuffer[bufferLength];
  memset(rbuffer, 0, bufferLength); // clear the previous message
  struct message response_message;
  int rcount = (*socket).recv(rbuffer, sizeof rbuffer);

  response_message.type = rbuffer[0];
  response_message.length = (sizeof rbuffer);
  response_message.payload = rbuffer;

  printf("recv %d [%.*s]\n", rcount, strstr(response_message.payload, "\r\n") - response_message.payload, response_message.payload);

  if (isCard)
  {
    if (strcmp(allow, response_message.payload) == 0)
    {
      turnOnGreenLight();
    }
    else if (strcmp(do_not_allow, response_message.payload) == 0)
    {
      turnOnRedLight();
    }
    ThisThread::sleep_for(3s);
  }
}

int checkIfServerIsDown(int scount)
{

  if (scount <= 0)
  {

    LedBlue = 1;
    LedGreen = 0;
    LedRed = 0; // on
    printf("Server is down.Please wait!\n");
    ThisThread::sleep_for(3s);
    return 1;
  }
  return 0;
}

void RFIDCommunication(int communication_failed, TCPSocket *sock)
{

  char card_id[5] = "@";
  LedBlue = 1;
  printf("Scan your tag...\n");

  // Init. RC522 Chip
  RfChip.PCD_Init();

  while (communication_failed == 0)
  {

    // Look for new cards
    if (!RfChip.PICC_IsNewCardPresent())
    {
      LedGreen = 1;
      LedRed = 1;
      LedBlue = !LedBlue;
      ThisThread::sleep_for(100);
      continue;
    }

    // Select one of the cards
    if (!RfChip.PICC_ReadCardSerial())
    {
      LedGreen = 1;
      LedRed = 1;
      LedBlue = !LedBlue;
      ThisThread::sleep_for(100);
      continue;
    }

    LedBlue = 1;

    // Print Card UID
    for (uint8_t i = 0; i < RfChip.uid.size; i++)
    {
      card_id[1 + i] = RfChip.uid.uidByte[i];
      // printf(" %X", RfChip.uid.uidByte[i]);
    }

    ThisThread::sleep_for(100);

    struct message card_message;

    card_message.type = card_id[0];
    card_message.length = (sizeof card_id);
    card_message.payload = card_id;

    int scount = sendMessageToServer(sock, card_message);

    // if server is down break and try again
    communication_failed = checkIfServerIsDown(scount);
    if (communication_failed)
    {
      break;
    }

    turnOffAllLights();
    recieveResponseFromServer(sock, 40, 1);
  }
}

int askForPublicKey(TCPSocket *socket)
{

  struct message hello_message;
  char hello_msg[] = "!Hello, I need public key.";

  hello_message.type = hello_msg[0];
  hello_message.length = (sizeof hello_msg);
  hello_message.payload = hello_msg;

  int rec = sendMessageToServer(socket, hello_message);
  int communication_failed = checkIfServerIsDown(rec);

  return communication_failed;
}

void generateAndEncryptAesKey(unsigned char aes_key[], char public_key[],size_t public_key_len,size_t aes_key_len)
{
  mbedtls_ctr_drbg_context ctr_drbg;
  mbedtls_entropy_context entropy;
  // unsigned char key[32];

  char *pers = "aes generate key";
  int ret;

  mbedtls_entropy_init(&entropy);

  mbedtls_ctr_drbg_init(&ctr_drbg);

  if ((ret = mbedtls_ctr_drbg_seed(&ctr_drbg, mbedtls_entropy_func, &entropy,
                                   (unsigned char *)pers, strlen(pers))) != 0)
  {
    printf(" failed\n ! mbedtls_ctr_drbg_init returned -0x%04x\n", -ret);
  }

  if ((ret = mbedtls_ctr_drbg_random(&ctr_drbg, aes_key, 32)) != 0)
  {
    printf(" failed\n ! mbedtls_ctr_drbg_random returned -0x%04x\n", -ret);
  }

  printf("Aes key is: ");
  print_array(aes_key, 32);
  //printf("key len:%d \n",private_key_len );
 // printf("public_key  [%.*s]\n", strstr(public_key, "\r\n") - public_key, public_key);
  print_byte_array(public_key,162);

  mbedtls_pk_context pk;

  mbedtls_pk_init(&pk);

  if ((ret = mbedtls_pk_parse_public_key(&pk, (unsigned char*)public_key, public_key_len )) != 0)
  {
    printf(" failed\n  ! mbedtls_pk_parse_public_key returned -0x%04x\n", -ret);
  }
  

  
  
  unsigned char buf[MBEDTLS_MPI_MAX_SIZE];
  size_t olen = 0;

  printf("\n  . Generating the encrypted value");
  fflush(stdout);

  if ((ret = mbedtls_pk_encrypt(&pk, aes_key, aes_key_len,buf, &olen, sizeof(buf),mbedtls_ctr_drbg_random, &ctr_drbg)) != 0)
  {
    printf(" failed\n  ! mbedtls_pk_encrypt returned -0x%04x\n", -ret);
  }

}

void receivePublicKey(TCPSocket *socket, char public_key[], int n)
{
  char rbuffer[n];
  memset(rbuffer, 0, n); // clear the previous message
  // struct message response_message;
  int rcount = (*socket).recv(rbuffer, n);
  
  printf("recv %d \n", rcount);

  memcpy(public_key, rbuffer, n);
   printf("public key is:\n");
  for (int i = 0; i < n; ++i) {
        printf("%X ", public_key[i]);
    }
  printf("\r\n");
  // printf("copy [%.*s]\n", strstr(public_key, "\r\n") - public_key, public_key);
}

int bringUpEthernetConnection(TCPSocket *socket)
{

  net.connect();

  // Show the network address
  SocketAddress a;
  net.get_ip_address(&a);
  printf("IP address: %s\n", a.get_ip_address() ? a.get_ip_address() : "None");

  // Open a socket on the network interface, and create a TCP connection

  (*socket).open(&net);

  net.gethostbyname("192.168.1.4", &a);
  a.set_port(8080);
  (*socket).connect(a);
  return 0;
}

int main()
{
  turnOnBlueLight();

  int communication_failed = 0;

  char device_id[17] = "#";
  findUniqueDeviceId(device_id);

  while (1)
  {

    printf("Door Lock System\n");
    printf("Connecting to server...\n");
    TCPSocket socket;
    int fail=bringUpEthernetConnection(&socket);
    if(fail){
      continue;
    }

    //------------CLIENT ASK FOR PUBLIC KEY------------
    //--

    communication_failed = askForPublicKey(&socket);

    if (communication_failed)
    {
      continue;
    }

    //----------CLIENT RECIEVE PUBLIC KEY--------------------
    int public_key_length=162;
    char public_key[public_key_length];
    size_t public_key_size = sizeof public_key / sizeof public_key[0];

    receivePublicKey(&socket, public_key, public_key_length);

    //print_byte_array(public_key,162);
    // printf("pub key is: [%.*s]\n", strstr(public_key, "\r\n") - public_key, public_key);

    //------------------------------------------------------

    unsigned char aes_key[32];
    size_t aes_key_size = sizeof aes_key / sizeof aes_key[0];
    generateAndEncryptAesKey(aes_key, public_key,public_key_size,aes_key_size);


    //----------CLIENT SEND DEVICE ID--------------------
    struct message device_message;

    device_message.type = device_id[0];
    device_message.length = (sizeof device_id);
    device_message.payload = device_id;

    int s = sendMessageToServer(&socket, device_message);

    communication_failed = checkIfServerIsDown(s);
    if (communication_failed)
    {
      continue;
    }

    recieveResponseFromServer(&socket, 256, 0);

    //---------------SCAN TAGS-----------------------

    RFIDCommunication(communication_failed, &socket);

    // Close the socket to return its memory and bring down the network interface
    socket.close();

    // Bring down the ethernet interface
    net.disconnect();
  }

  printf("Done\n");
}
