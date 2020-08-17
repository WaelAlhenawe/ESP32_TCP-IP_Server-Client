#include <WiFi.h>
#include <Arduino.h>
#include <esp32-hal.h>
#include <IPAddress.h>
#include <WiFiClient.h>

#include <RSA.h>
#include <SHA1.h>
#include <AES128.h>

#define SSID "Telia-ED8AC9"
#define PASSWORD "05162D814F"

#define PORT (12345U)
#define SERVER "192.168.1.165"

#define LENGTH (16U)
#define HASH_SIZE (20U)
#define BUFFER_SIZE (128U)

// The keys and the exponent are generated by https://csfieldguide.org.nz/en/interactives/rsa-key-generator/

//static uint8_t exponent[] = {0x00, 0x01, 0x00, 0x01};
static uint8_t client_public_key[RSA_SIZE] = {
    0xDB, 0x44, 0xDD, 0xA4, 0xB7, 0xAB, 0x9D, 0x86, 0x2B, 0xBD, 0xC1, 0xFD, 0x67, 0xC9, 0x0B, 0xAF,
    0x05, 0x76, 0x3E, 0x4E, 0xD3, 0xD1, 0xDF, 0x9B, 0x7A, 0x75, 0x6E, 0x4C, 0x5F, 0x63, 0x63, 0x75};
static uint8_t client_private_key[RSA_SIZE] = {
    0x5B, 0xF4, 0x39, 0x6F, 0x46, 0x87, 0x75, 0xFC, 0x3A, 0x83, 0xCD, 0xC2, 0xD3, 0xAF, 0x80, 0x72,
    0x12, 0x98, 0x99, 0x0E, 0x0F, 0x43, 0xA2, 0x7B, 0x47, 0xB1, 0x3C, 0x23, 0xC9, 0x99, 0x64, 0x81};
static uint8_t public_key_server[RSA_SIZE] = {
    0xC3, 0xA5, 0x4E, 0x87, 0xAD, 0xC6, 0xA4, 0x02, 0x11, 0x0B, 0xF2, 0x75, 0xE3, 0xB6, 0x6D, 0xE6,
    0x55, 0xA0, 0x17, 0x60, 0x16, 0xC2, 0x12, 0x58, 0xA9, 0xC6, 0xF5, 0x91, 0xCD, 0xB7, 0xA7, 0xA9};
// static uint8_t private_key_server[RSA_SIZE] ={
// 0x56, 0x29, 0x30, 0xE2, 0x73, 0xD7, 0x6D, 0x57, 0x33, 0xA6, 0xAD, 0x4A, 0xD9, 0xD3, 0xF7, 0xA5,
// 0x98, 0xF3, 0xFA, 0x07, 0x64, 0x7D, 0xE5, 0xE4, 0x4B, 0x13, 0x5C, 0x90, 0x38, 0xF4, 0x3B, 0x59 };

struct conction_flow{
  bool auth_pass = false;
  bool aes_pass = false;
};

static conction_flow status;

enum client_mes_type
{
    AUTH = 1,
    AES_KEY,
    REQUET
};

enum server_mes_type
{
    AUTH_OK = 1,
    AES_OK,
    DONE,
    RE_AUTH,
    ERROR,
    RE_DO
};

struct res_info
{
    server_mes_type key;
    uint8_t message[RSA_SIZE] = {};
    uint8_t hash_value[HASH_SIZE] = {};
};
static void print_data(const uint8_t *data, uint8_t size)
{
    for (uint8_t i = 0; i < size; i++)
    {
        Serial.printf("%02X ", data[i]);
    }
    Serial.println();
}

static void services_menu(uint8_t *massage)
{
    bool menu_apper = true, flag = true, finish = false;
    while (flag)
    {

        while (menu_apper)
        {
            menu_apper = false;

            Serial.println("------------------------------");
            Serial.println("Please Choose Type Of Service.");
            Serial.println("------------------------------");
            Serial.println("(1) For Set Light ON.");
            Serial.println("(2) For Set Light OFF.");
            Serial.println("(3) For Check Light Status.");
            Serial.println("(4) For Get Server Temperature.");
            Serial.println("(5) For Disconnect.");

            Serial.print("Your Choice: ");
        }

        while (!finish)
        {
            //Serial.println("befor Serial Available");

            if (Serial.available())
            {
                //Serial.println("Serial Available");

                char chr = Serial.read();
                if (chr == '\r')
                {
                    //Serial.println("Serial Available");
                    chr = Serial.read();
                }
                //Serial.println("Serial 11111");

                if (chr && chr != '\n')
                {
                    Serial.println(chr);
                    // Serial.println("Serial 7777");
                    massage[0] = (uint8_t)chr;
                    Serial.println(massage[0]);
                    //Serial.println(*counter);
                    massage[1] = (uint8_t)'\n';
                    Serial.println(massage[1]);

                    finish = true;
                    flag = false;
                    //Serial.println("Serial 888888");
                }
            }
        }
    }
    Serial.println("After Serial Available");

    if (!(massage[0] > 0 + '0' && massage[0] < 6 + '0'))
    {
        menu_apper = true;
        const char wrong_input[26] = "PLEASE ENTER RIGHT CHOICE";
        for (int i = 0; i < strlen(wrong_input); i++)
        {
            Serial.printf("%c", wrong_input[i]);
            delay(500);
        }
        Serial.println();
    }
    else
    {
        flag = false;
    }
}

//static uint8_t client_public_key[RSA_SIZE] ={};
//static uint8_t client_private_key[RSA_SIZE] ={};
static WiFiClient client;
static uint8_t tx_counter = 0U;
static char tx_buffer[BUFFER_SIZE] = {};
client_mes_type operation_type = client_mes_type(AUTH);
const uint8_t *key;

static res_info response_details;
static res_info response_parsing(uint8_t *message)
{
    res_info temp;
    uint8_t encrypted_massage_size;
    //uint8_t decrypted_massage_size;
    temp.key = (server_mes_type)message[0];
    // Debug
    /*Serial.print("The key is : ");
     Serial.println(temp.key);*/

    if (temp.key == server_mes_type(AUTH_OK) || temp.key == server_mes_type(AES_OK))
    {
        encrypted_massage_size = RSA_SIZE;
    }
    else
    {
        encrypted_massage_size = AES_KEY_SIZE;
    }

    for (int i = 1; i < encrypted_massage_size + 1; i++)
    {
        temp.message[i - 1] = message[i];
    }
    // Debug
    Serial.print("Encrypted message: ");
    print_data(temp.message, encrypted_massage_size);

    for (int i = 1 + encrypted_massage_size; i < encrypted_massage_size + HASH_SIZE + 1; i++)
    {
        temp.hash_value[i - (1 + encrypted_massage_size)] = message[i];
    }
    Serial.print("Hash is: ");
    print_data(temp.hash_value, HASH_SIZE);
    // Debug
    /*Serial.print("The hash is : ");
    Serial.println((char*)temp.hash_value);
    Serial.println(" ");
    Serial.println(" ");*/
    return temp;
}
static bool check_hash(uint8_t *mes, const uint8_t *hash_res, uint8_t size)
{
    bool flag = true;
    uint8_t temp_hash[HASH_SIZE] = {};
    sha1(mes, size, temp_hash);

    // Debug
    Serial.print("The mesaseg: ");
    print_data(mes, size);

    Serial.print("New hash is: ");
    print_data(temp_hash, HASH_SIZE);

    Serial.print("Old hash is: ");
    print_data(hash_res, HASH_SIZE);

    for (int i = 0; i < HASH_SIZE; i++)
    {
        if (!(hash_res[i] == temp_hash[i]))
        {
            flag = false;

            /*Serial.print((char)hash_res[i]);
            Serial.print(" ");
            Serial.println((char)temp_hash[i]);
            // Debug
            Serial.println(i);
            Serial.println(i -( (strlen((char*)mes) - HASH_SIZE )));
            Serial.println((char)mes[i]);
            Serial.println((char)hash[i -( (strlen((char*)mes) - HASH_SIZE  ))]);
            Serial.println("Hash Code Fail!!!");*/
            break;
        }
    }
    return flag;
}

static uint8_t build_message(const client_mes_type type, uint8_t *data, uint8_t data_size, char *buffer)
{
    uint8_t i = 0;
    uint8_t hash[HASH_SIZE] = {};

    Serial.println("I AM ON BUILD : ");

    buffer[i] = type;

    // Debug
    Serial.print("data message: ");
    Serial.println((char *)data);
    //Serial.println(data_size);

    if (type == client_mes_type(AUTH) || type == client_mes_type(AES_KEY))
    {
        uint8_t encrypted[RSA_SIZE] = {};
        rsa_public_encrypt(data, data_size, public_key_server, encrypted);

        for (i = 1; i < RSA_SIZE + 1; i++)
        {
            buffer[i] = encrypted[i - 1];
        }
        sha1(encrypted, RSA_SIZE, hash);

        // Debug
        Serial.print("Hash builded is: ");
        print_data(hash, sizeof(hash));

        for (i = 1 + RSA_SIZE; i < HASH_SIZE + (1 + RSA_SIZE); i++)
        {
            buffer[i] = hash[i - (1 + RSA_SIZE)];
        }
        buffer[i] = '\0';
    }
    else
    {
        Serial.println("AES ENNNNNN");
        uint8_t encrypted[AES_CIPHER_SIZE] = {};
        uint8_t decrypted[AES_BLOCK_SIZE] = {};

        aes128_encrypt(data, data_size, encrypted);
        print_data(encrypted, sizeof(encrypted));
        print_data(encrypted, sizeof(encrypted));

        //aes128_decrypt(encrypted, decrypted);

        Serial.print("Decrypted AES: ");
        Serial.println((char *)decrypted);
        for (i = 1; i < AES_CIPHER_SIZE + 1; i++)
        {
            buffer[i] = encrypted[i - 1];
        }
        Serial.print("Bulided Encreypted massage ");
        print_data(encrypted, sizeof(encrypted));
        sha1(encrypted, AES_CIPHER_SIZE, hash);

        // Debug
        Serial.print("Hash AES BULID is: ");
        print_data(hash, sizeof(hash));

        for (i = 1 + AES_CIPHER_SIZE; i < HASH_SIZE + (1 + AES_CIPHER_SIZE); i++)
        {
            buffer[i] = hash[i - (1 + AES_CIPHER_SIZE)];
        }
        buffer[i] = '\0';
        Serial.println("tx_buffer inside : ");

        print_data((uint8_t *)tx_buffer, sizeof(tx_buffer));
    }
    // Debug
    //Serial.print("Encrypted message: ");
    //print_data(encrypted, sizeof(encrypted));
    return i;
}

/*// Set your Static IP address

IPAddress local_IP(172, 31, 31, 59);
// Set your Gateway IP address
//IPAddress gateway(172, 31, 1, 1);

//IPAddress subnet(255, 255, 0, 0);
// Set your Gateway IP address
IPAddress gateway(172, 31, 31, 1);

IPAddress subnet(255, 255, 0, 0);
IPAddress primaryDNS(8, 8, 8, 8);   //optional
IPAddress secondaryDNS(8, 8, 4, 4); //optional
//extern "C" // Extern C is used when we are using a funtion written in "C" language in a C++ code.
//{
//uint8_t temprature_sens_read(); // This function is written in C language
//}

//uint8_t temprature_sens_read();
*/

void setup()
{
    /*   // Configures static IP address
   if (!WiFi.config(local_IP, gateway, subnet, primaryDNS, secondaryDNS)) {
     Serial.println("STA Failed to configure");
   }*/

    Serial.begin(9600);
    while (!Serial)
    {
        delay(100);
    }
    Serial.print("\nIP Address: ");

    WiFi.begin(SSID, PASSWORD);

    while (WiFi.status() != WL_CONNECTED)
    {
        delay(500);
        Serial.print(".");
    }

    Serial.print("\nIP Address: ");
    Serial.println(WiFi.localIP());
}

void loop()
{
    //rsa_generate_keys(client_public_key, client_private_key);

    uint8_t auth_key[RSA_BLOCK_SIZE] = "kp2-5v8/B?E(H+VmY3wA";

    Serial.println("\n-------------------------");
    Serial.print("Original message: ");
    Serial.print((char *)auth_key);
    Serial.println();
    Serial.println(operation_type);

    if (status.aes_pass == false) 
    {

        uint8_t auth_hash[HASH_SIZE] = {};
        sha1(auth_key,RSA_BLOCK_SIZE,auth_hash);
        Serial.print("First hash is: ");
        print_data((uint8_t *)auth_hash, HASH_SIZE);
        
        uint8_t rsa_hash_auth [RSA_SIZE]= {};
        rsa_private_encrypt(auth_hash,HASH_SIZE, client_public_key, client_private_key, rsa_hash_auth);
        for (tx_counter = 0 ; tx_counter < RSA_SIZE ; tx_counter ++)
        {
            tx_buffer[tx_counter] = rsa_hash_auth[tx_counter];
        }

        uint8_t auth_hash_hash[HASH_SIZE] = {};
        sha1(rsa_hash_auth,RSA_SIZE,auth_hash_hash);
        for (tx_counter = tx_counter ; tx_counter < RSA_SIZE + HASH_SIZE ; tx_counter ++)
        {
            tx_buffer[tx_counter] = auth_hash_hash[tx_counter - RSA_SIZE];
        }

        tx_buffer[tx_counter] = '\0';

        Serial.print("The whole message is: ");
        print_data((uint8_t *)tx_buffer, tx_counter);

        
    }

    if (operation_type == client_mes_type(AES_KEY))
    {
        key = aes128_init_key(NULL);
        Serial.print("AES Key: ");
        print_data(key, AES_KEY_SIZE);

        tx_counter = build_message(client_mes_type(AES_KEY), (uint8_t *)key, AES_KEY_SIZE, tx_buffer);
    }

    if (operation_type == client_mes_type(REQUET))
    {

        Serial.println("I am on REQUEST");

        uint8_t choice[2] = {};
        services_menu(choice);
        Serial.println("Menu Passed");
        tx_counter = build_message(client_mes_type(REQUET), choice, sizeof(choice), tx_buffer);

        /*uint8_t temp_encrypt[AES_CIPHER_SIZE] ={};
        uint8_t temp_decrypt[AES_BLOCK_SIZE] ={};

        for (uint8_t i = 1; i < AES_CIPHER_SIZE+1; i++)
        {
                temp_encrypt[i-1] = tx_buffer[i];
            
        }
        aes128_decrypt(temp_encrypt, temp_decrypt);
        Serial.print("TEST TEST TSET::::");
        Serial.println((char*) temp_decrypt);*/
    }

    if (tx_buffer[tx_counter] == '\0')
    {
        Serial.println("I am here");

        client.connect(SERVER, PORT);

        if (client.connected())
        {
            Serial.println("I am here2");

            if (client.write(tx_buffer) == strlen(tx_buffer))
            {

                Serial.println("I am here3");

                char rx_buffer[BUFFER_SIZE] = {};

                while (!strlen((char *)rx_buffer))
                {
                    if (client.connected())
                    {

                        client.read((uint8_t *)rx_buffer, (size_t)sizeof(rx_buffer));
                        if (strlen((char *)rx_buffer))
                        {

                            Serial.print("rx_buffer recived is: ");

                            print_data((uint8_t *)rx_buffer, strlen(rx_buffer));
                            response_details = response_parsing((uint8_t *)rx_buffer);
                            uint8_t massage_size;
                            if (response_details.key == server_mes_type(AUTH_OK) || response_details.key == server_mes_type(AES_OK))
                            {
                                massage_size = RSA_SIZE;
                            }
                            else
                            {
                                massage_size = AES_KEY_SIZE;
                            }

                            // Debug
                            Serial.print("Encrypted message out said: ");
                            print_data(response_details.message, sizeof(response_details.message));
                            uint8_t decrypt[RSA_BLOCK_SIZE] = {};
                            if (check_hash(response_details.message, response_details.hash_value, massage_size))
                            {
                                switch (response_details.key)
                                {
                                case server_mes_type(AUTH_OK):
                                {

                                    if (rsa_private_decrypt(response_details.message, client_public_key, client_private_key, decrypt))
                                    {
                                        operation_type = client_mes_type(AES_KEY);
                                    }

                                    Serial.print("The encrypted res massage is: ");

                                    Serial.println((char *)decrypt);

                                    break;
                                }
                                case server_mes_type(AES_OK):

                                    if (rsa_private_decrypt(response_details.message, client_public_key, client_private_key, decrypt))
                                    {
                                        operation_type = client_mes_type(REQUET);
                                    }
                                    //operation_type = client_mes_type(REQUET);
                                    Serial.print("The encrypted RSA massage is: ");

                                    Serial.println((char *)decrypt);

                                    break;
                                case server_mes_type(DONE):
                                {

                                    //uint8_t text[AES_BLOCK_SIZE] ={};
                                    Serial.println("I am on DONE: ");
                                    aes128_decrypt(response_details.message, decrypt);
                                    Serial.print("The encrypted res massage is: ");

                                    Serial.println((char *)decrypt);
                                    //operation_type = client_mes_type(AES_KEY);
                                }
                                break;
                                case client_mes_type(RE_AUTH):
                                    operation_type = client_mes_type(AUTH);
                                    Serial.println("I am on RE_AUTH: ");
                                    aes128_decrypt(response_details.message, decrypt);
                                    Serial.print("The encrypted res massage is: ");

                                    Serial.println((char *)decrypt);
                                    break;
                                case client_mes_type(RE_DO):

                                    break;
                                }
                            }
                        }
                    }
                    else
                    {

                        client.stop();
                        client.connect(SERVER, PORT);
                    }
                }

                client.stop();
                tx_counter = 0;
                memset(tx_buffer, 0, sizeof(tx_buffer));
            }
        }
        else
        {
            Serial.print(".");
        }
    }
}