#include <string.h>
// #include "ws2812_i2s.h"
// #include "i2s_dma/i2s_dma.h"

// #include "freertos/FreeRTOS.h"
// #include "freertos/task.h"
#include "esp_log.h"

#include "driver/i2c.h"

#define SSD1306_Address               0x3C
#define SSD1306_Command_Mode          0x80
#define SSD1306_Data_Mode             0x40
#define SSD1306_Display_Off_Cmd       0xAE
#define SSD1306_Display_On_Cmd        0xAF
#define SSD1306_Normal_Display_Cmd    0xA6
#define SSD1306_Inverse_Display_Cmd   0xA7
#define SSD1306_Activate_Scroll_Cmd   0x2F
#define SSD1306_Dectivate_Scroll_Cmd  0x2E
#define SSD1306_Set_Brightness_Cmd    0x81


#define SSD1306_MEMORYMODE          0x20 ///< See datasheet
#define SSD1306_COLUMNADDR          0x21 ///< See datasheet
#define SSD1306_PAGEADDR            0x22 ///< See datasheet
#define SSD1306_SETCONTRAST         0x81 ///< See datasheet
#define SSD1306_CHARGEPUMP          0x8D ///< See datasheet
#define SSD1306_SEGREMAP            0xA0 ///< See datasheet
#define SSD1306_DISPLAYALLON_RESUME 0xA4 ///< See datasheet
#define SSD1306_DISPLAYALLON        0xA5 ///< Not currently used
#define SSD1306_NORMALDISPLAY       0xA6 ///< See datasheet
#define SSD1306_INVERTDISPLAY       0xA7 ///< See datasheet
#define SSD1306_SETMULTIPLEX        0xA8 ///< See datasheet
#define SSD1306_DISPLAYOFF          0xAE ///< See datasheet
#define SSD1306_DISPLAYON           0xAF ///< See datasheet
#define SSD1306_COMSCANINC          0xC0 ///< Not currently used
#define SSD1306_COMSCANDEC          0xC8 ///< See datasheet
#define SSD1306_SETDISPLAYOFFSET    0xD3 ///< See datasheet
#define SSD1306_SETDISPLAYCLOCKDIV  0xD5 ///< See datasheet
#define SSD1306_SETPRECHARGE        0xD9 ///< See datasheet
#define SSD1306_SETCOMPINS          0xDA ///< See datasheet
#define SSD1306_SETVCOMDETECT       0xDB ///< See datasheet

#define SSD1306_SETLOWCOLUMN        0x00 ///< Not currently used
#define SSD1306_SETHIGHCOLUMN       0x10 ///< Not currently used
#define SSD1306_SETSTARTLINE        0x40 ///< See datasheet


// 8x8 Font ASCII 32 - 127 Implemented
// Users can modify this to support more characters (glyphs)

static uint8_t font8x8[] =
{
//   0x08,                                     // width
//   0x08,                                     // height

  0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,  // <space>
  0x00,0x00,0x00,0x00,0x5F,0x00,0x00,0x00,  // !
  0x00,0x00,0x00,0x03,0x00,0x03,0x00,0x00,  // "
  0x00,0x24,0x7E,0x24,0x24,0x7E,0x24,0x00,  // #
  0x00,0x2E,0x2A,0x7F,0x2A,0x3A,0x00,0x00,  // $
  0x00,0x46,0x26,0x10,0x08,0x64,0x62,0x00,  // %
  0x00,0x20,0x54,0x4A,0x54,0x20,0x50,0x00,  // &
  0x00,0x00,0x00,0x04,0x02,0x00,0x00,0x00,  // '
  0x00,0x00,0x00,0x3C,0x42,0x00,0x00,0x00,  // (
  0x00,0x00,0x00,0x42,0x3C,0x00,0x00,0x00,  // )
  0x00,0x10,0x54,0x38,0x54,0x10,0x00,0x00,  // *
  0x00,0x10,0x10,0x7C,0x10,0x10,0x00,0x00,  // +
  0x00,0x00,0x00,0x80,0x60,0x00,0x00,0x00,  // ,
  0x00,0x10,0x10,0x10,0x10,0x10,0x00,0x00,  // -
  0x00,0x00,0x00,0x60,0x60,0x00,0x00,0x00,  // .
  0x00,0x40,0x20,0x10,0x08,0x04,0x00,0x00,  // /

  0x3C,0x62,0x52,0x4A,0x46,0x3C,0x00,0x00,  // 0
  0x44,0x42,0x7E,0x40,0x40,0x00,0x00,0x00,  // 1
  0x64,0x52,0x52,0x52,0x52,0x4C,0x00,0x00,  // 2
  0x24,0x42,0x42,0x4A,0x4A,0x34,0x00,0x00,  // 3
  0x30,0x28,0x24,0x7E,0x20,0x20,0x00,0x00,  // 4
  0x2E,0x4A,0x4A,0x4A,0x4A,0x32,0x00,0x00,  // 5
  0x3C,0x4A,0x4A,0x4A,0x4A,0x30,0x00,0x00,  // 6
  0x02,0x02,0x62,0x12,0x0A,0x06,0x00,0x00,  // 7
  0x34,0x4A,0x4A,0x4A,0x4A,0x34,0x00,0x00,  // 8
  0x0C,0x52,0x52,0x52,0x52,0x3C,0x00,0x00,  // 9
  0x00,0x00,0x00,0x48,0x00,0x00,0x00,0x00,  // :
  0x00,0x00,0x80,0x64,0x00,0x00,0x00,0x00,  // ;
  0x00,0x00,0x10,0x28,0x44,0x00,0x00,0x00,  // <
  0x00,0x28,0x28,0x28,0x28,0x28,0x00,0x00,  // =
  0x00,0x00,0x44,0x28,0x10,0x00,0x00,0x00,  // >
  0x00,0x04,0x02,0x02,0x52,0x0A,0x04,0x00,  // ?

  0x00,0x3C,0x42,0x5A,0x56,0x5A,0x1C,0x00,  // @
  0x7C,0x12,0x12,0x12,0x12,0x7C,0x00,0x00,  // A
  0x7E,0x4A,0x4A,0x4A,0x4A,0x34,0x00,0x00,  // B
  0x3C,0x42,0x42,0x42,0x42,0x24,0x00,0x00,  // C
  0x7E,0x42,0x42,0x42,0x24,0x18,0x00,0x00,  // D
  0x7E,0x4A,0x4A,0x4A,0x4A,0x42,0x00,0x00,  // E
  0x7E,0x0A,0x0A,0x0A,0x0A,0x02,0x00,0x00,  // F
  0x3C,0x42,0x42,0x52,0x52,0x34,0x00,0x00,  // G
  0x7E,0x08,0x08,0x08,0x08,0x7E,0x00,0x00,  // H
  0x00,0x42,0x42,0x7E,0x42,0x42,0x00,0x00,  // I
  0x30,0x40,0x40,0x40,0x40,0x3E,0x00,0x00,  // J
  0x7E,0x08,0x08,0x14,0x22,0x40,0x00,0x00,  // K
  0x7E,0x40,0x40,0x40,0x40,0x40,0x00,0x00,  // L
  0x7E,0x04,0x08,0x08,0x04,0x7E,0x00,0x00,  // M
  0x7E,0x04,0x08,0x10,0x20,0x7E,0x00,0x00,  // N
  0x3C,0x42,0x42,0x42,0x42,0x3C,0x00,0x00,  // O

  0x7E,0x12,0x12,0x12,0x12,0x0C,0x00,0x00,  // P
  0x3C,0x42,0x52,0x62,0x42,0x3C,0x00,0x00,  // Q
  0x7E,0x12,0x12,0x12,0x32,0x4C,0x00,0x00,  // R
  0x24,0x4A,0x4A,0x4A,0x4A,0x30,0x00,0x00,  // S
  0x02,0x02,0x02,0x7E,0x02,0x02,0x02,0x00,  // T
  0x3E,0x40,0x40,0x40,0x40,0x3E,0x00,0x00,  // U
  0x1E,0x20,0x40,0x40,0x20,0x1E,0x00,0x00,  // V
  0x3E,0x40,0x20,0x20,0x40,0x3E,0x00,0x00,  // W
  0x42,0x24,0x18,0x18,0x24,0x42,0x00,0x00,  // X
  0x02,0x04,0x08,0x70,0x08,0x04,0x02,0x00,  // Y
  0x42,0x62,0x52,0x4A,0x46,0x42,0x00,0x00,  // Z
  0x00,0x00,0x7E,0x42,0x42,0x00,0x00,0x00,  // [
  0x00,0x04,0x08,0x10,0x20,0x40,0x00,0x00,  // <backslash>
  0x00,0x00,0x42,0x42,0x7E,0x00,0x00,0x00,  // ]
  0x00,0x08,0x04,0x7E,0x04,0x08,0x00,0x00,  // ^
  0x80,0x80,0x80,0x80,0x80,0x80,0x80,0x00,  // _

  0x3C,0x42,0x99,0xA5,0xA5,0x81,0x42,0x3C,  // `
  0x00,0x20,0x54,0x54,0x54,0x78,0x00,0x00,  // a
  0x00,0x7E,0x48,0x48,0x48,0x30,0x00,0x00,  // b
  0x00,0x00,0x38,0x44,0x44,0x44,0x00,0x00,  // c
  0x00,0x30,0x48,0x48,0x48,0x7E,0x00,0x00,  // d
  0x00,0x38,0x54,0x54,0x54,0x48,0x00,0x00,  // e
  0x00,0x00,0x00,0x7C,0x0A,0x02,0x00,0x00,  // f
  0x00,0x18,0xA4,0xA4,0xA4,0xA4,0x7C,0x00,  // g
  0x00,0x7E,0x08,0x08,0x08,0x70,0x00,0x00,  // h
  0x00,0x00,0x00,0x48,0x7A,0x40,0x00,0x00,  // i
  0x00,0x00,0x40,0x80,0x80,0x7A,0x00,0x00,  // j
  0x00,0x7E,0x18,0x24,0x40,0x00,0x00,0x00,  // k
  0x00,0x00,0x00,0x3E,0x40,0x40,0x00,0x00,  // l
  0x00,0x7C,0x04,0x78,0x04,0x78,0x00,0x00,  // m
  0x00,0x7C,0x04,0x04,0x04,0x78,0x00,0x00,  // n
  0x00,0x38,0x44,0x44,0x44,0x38,0x00,0x00,  // o

  0x00,0xFC,0x24,0x24,0x24,0x18,0x00,0x00,  // p
  0x00,0x18,0x24,0x24,0x24,0xFC,0x80,0x00,  // q
  0x00,0x00,0x78,0x04,0x04,0x04,0x00,0x00,  // r
  0x00,0x48,0x54,0x54,0x54,0x20,0x00,0x00,  // s
  0x00,0x00,0x04,0x3E,0x44,0x40,0x00,0x00,  // t
  0x00,0x3C,0x40,0x40,0x40,0x3C,0x00,0x00,  // u
  0x00,0x0C,0x30,0x40,0x30,0x0C,0x00,0x00,  // v
  0x00,0x3C,0x40,0x38,0x40,0x3C,0x00,0x00,  // w
  0x00,0x44,0x28,0x10,0x28,0x44,0x00,0x00,  // x
  0x00,0x1C,0xA0,0xA0,0xA0,0x7C,0x00,0x00,  // y
  0x00,0x44,0x64,0x54,0x4C,0x44,0x00,0x00,  // z
  0x00,0x08,0x08,0x76,0x42,0x42,0x00,0x00,  // {
  0x00,0x00,0x00,0x7E,0x00,0x00,0x00,0x00,  // |
  0x00,0x42,0x42,0x76,0x08,0x08,0x00,0x00,  // }
  0x00,0x00,0x04,0x02,0x04,0x02,0x00,0x00,  // ~
};






// #define SSD1306_EXTERNALVCC         0x01 ///< External display voltage source
// #define SSD1306_SWITCHCAPVCC        0x02 ///< Gen. display voltage from 3.3V

const int SSD1306_EXTERNALVCC = 0;

#define SSD1306_RIGHT_HORIZONTAL_SCROLL              0x26 ///< Init rt scroll
#define SSD1306_LEFT_HORIZONTAL_SCROLL               0x27 ///< Init left scroll
#define SSD1306_VERTICAL_AND_RIGHT_HORIZONTAL_SCROLL 0x29 ///< Init diag scroll
#define SSD1306_VERTICAL_AND_LEFT_HORIZONTAL_SCROLL  0x2A ///< Init diag scroll
#define SSD1306_DEACTIVATE_SCROLL                    0x2E ///< Stop scroll
#define SSD1306_ACTIVATE_SCROLL                      0x2F ///< Start scroll
#define SSD1306_SET_VERTICAL_SCROLL_AREA             0xA3 ///< Set scroll range


// #define I2C_EXAMPLE_MASTER_SCL_IO          19               /*!< gpio number for I2C master clock */
// #define I2C_EXAMPLE_MASTER_SDA_IO          18               /*!< gpio number for I2C master data  */
#define I2C_EXAMPLE_MASTER_TX_BUF_DISABLE  0                /*!< I2C master do not need buffer */
#define I2C_EXAMPLE_MASTER_RX_BUF_DISABLE  0                /*!< I2C master do not need buffer */
#define I2C_EXAMPLE_MASTER_FREQ_HZ         100000           /*!< I2C master clock frequency */

#define ACK_CHECK_EN                       0x1              /*!< I2C master will check ack from slave*/
#define ACK_CHECK_DIS                      0x0              /*!< I2C master will not check ack from slave */

uint8_t ssd1306_buffer[128*32/8];

void ssd1306_sendCommand(unsigned char command)
{
//   Wire.beginTransmission(SSD1306_Address);    // begin I2C communication
//   Wire.write(SSD1306_Command_Mode);           // Set OLED Command mode
//   Wire.write(command);
//   Wire.endTransmission();                       // End I2C communication

    int ret;
    i2c_cmd_handle_t cmd = i2c_cmd_link_create();
    i2c_master_start(cmd);
    i2c_master_write_byte(cmd, SSD1306_Address << 1 | I2C_MASTER_WRITE, ACK_CHECK_EN);
    i2c_master_write_byte(cmd, 0, ACK_CHECK_EN);
    i2c_master_write_byte(cmd, command, ACK_CHECK_EN);
    i2c_master_stop(cmd);
    ret = i2c_master_cmd_begin(I2C_NUM_1, cmd, 1000 / portTICK_RATE_MS);
    i2c_cmd_link_delete(cmd);
    if (ret != ESP_OK) {
        printf("Failed to send I2C command %02X.\n", command);
    }
}


void ssd1306_sendCommandPtr(unsigned char *Command, int len)
{
    int ret;
    i2c_cmd_handle_t cmd = i2c_cmd_link_create();
    i2c_master_start(cmd);
    i2c_master_write_byte(cmd, SSD1306_Address << 1 | I2C_MASTER_WRITE, ACK_CHECK_EN);
    i2c_master_write_byte(cmd, 0, ACK_CHECK_EN);
    for(int i=0; i<len; i++) {
        i2c_master_write_byte(cmd, Command[i], ACK_CHECK_EN);
    }
    i2c_master_stop(cmd);
    ret = i2c_master_cmd_begin(I2C_NUM_1, cmd, 1000 / portTICK_RATE_MS);
    i2c_cmd_link_delete(cmd);
    if (ret != ESP_OK) {
        printf("Failed to send I2C command.\n");
    }
}

void ssd1306_sendData(unsigned char Data)
{
    int ret;
    i2c_cmd_handle_t cmd = i2c_cmd_link_create();
    i2c_master_start(cmd);
    i2c_master_write_byte(cmd, SSD1306_Address << 1 | I2C_MASTER_WRITE, ACK_CHECK_EN);
    i2c_master_write_byte(cmd, SSD1306_Data_Mode, ACK_CHECK_EN);
    i2c_master_write_byte(cmd, Data, ACK_CHECK_EN);
    i2c_master_stop(cmd);
    ret = i2c_master_cmd_begin(I2C_NUM_1, cmd, 1000 / portTICK_RATE_MS);
    i2c_cmd_link_delete(cmd);
    if (ret != ESP_OK) {
        printf("Failed to send I2C data %02X.\n", Data);
    }


    //  Wire.beginTransmission(SSD1306_Address); // begin I2C transmission
    //  Wire.write(SSD1306_Data_Mode);            // data mode
    //  Wire.write(Data);
    //  Wire.endTransmission();                    // stop I2C transmission
}

void ssd1306_sendDataPtr(unsigned char *Data, int len)
{
    int ret;
    i2c_cmd_handle_t cmd = i2c_cmd_link_create();
    i2c_master_start(cmd);
    i2c_master_write_byte(cmd, SSD1306_Address << 1 | I2C_MASTER_WRITE, ACK_CHECK_EN);
    i2c_master_write_byte(cmd, SSD1306_Data_Mode, ACK_CHECK_EN);
    for(int i=0; i<len; i++) {
        i2c_master_write_byte(cmd, Data[i], ACK_CHECK_EN);
    }
    i2c_master_stop(cmd);
    ret = i2c_master_cmd_begin(I2C_NUM_1, cmd, 1000 / portTICK_RATE_MS);
    i2c_cmd_link_delete(cmd);
    if (ret != ESP_OK) {
        printf("Failed to send I2C data.\n");
    }
}


void ssd1306_setTextXY(unsigned char row, unsigned char col)
{
    // sendCommand(0xB0 + row);                          //set page address
    // sendCommand(0x00 + (m_font_width*col & 0x0F));    //set column lower addr
    // sendCommand(0x10 + ((m_font_width*col>>4)&0x0F)); //set column higher addr
}

void ssd1306_clearDisplay()
{
  unsigned char i,j;
  ssd1306_sendCommand(SSD1306_Display_Off_Cmd);     //display off
//   for(j=0;j<8;j++)
//   {    
//     setTextXY(j,0);    
//     {
//       for(i=0;i<16;i++)  //clear all columns
//       {
//         putChar(' ');    
//       }
//     }
//   }
  ssd1306_sendCommand(SSD1306_Display_On_Cmd);     //display on
//   setTextXY(0,0);    
}

void display_init(uint8_t sda_pin, uint8_t scl_pin) {

        printf("Initializing I2C display...\n");

    int i2c_master_port = I2C_NUM_1;
    i2c_config_t conf;
    conf.mode = I2C_MODE_MASTER;
    conf.sda_io_num = sda_pin;
    conf.sda_pullup_en = GPIO_PULLUP_ENABLE;
    conf.scl_io_num = scl_pin;
    conf.scl_pullup_en = GPIO_PULLUP_ENABLE;
    conf.master.clk_speed = I2C_EXAMPLE_MASTER_FREQ_HZ;
    i2c_param_config(i2c_master_port, &conf);
    i2c_driver_install(i2c_master_port, conf.mode, I2C_EXAMPLE_MASTER_RX_BUF_DISABLE, I2C_EXAMPLE_MASTER_TX_BUF_DISABLE, 0);

    static const uint8_t init1[] = {
        SSD1306_DISPLAYOFF,                   // 0xAE
        SSD1306_SETDISPLAYCLOCKDIV,           // 0xD5
        0x80,                                 // the suggested ratio 0x80
        SSD1306_SETMULTIPLEX };               // 0xA8
    ssd1306_sendCommandPtr(init1, sizeof(init1));
    ssd1306_sendCommand(32 - 1);

    static const uint8_t init2[] = {
        SSD1306_SETDISPLAYOFFSET,             // 0xD3
        0x0,                                  // no offset
        SSD1306_SETSTARTLINE | 0x0,           // line #0
        SSD1306_CHARGEPUMP };                 // 0x8D
    ssd1306_sendCommandPtr(init2, sizeof(init2));

    ssd1306_sendCommand(/* (vccstate == SSD1306_EXTERNALVCC) ? 0x10 :*/ 0x14);

    static const uint8_t init3[] = {
        SSD1306_MEMORYMODE,                   // 0x20
        0x00,                                 // 0x0 act like ks0108
        SSD1306_SEGREMAP | 0x1,
        SSD1306_COMSCANDEC };
    ssd1306_sendCommandPtr(init3, sizeof(init3));

    static const uint8_t init4a[] = {
        SSD1306_SETCOMPINS,                 // 0xDA
        0x02,
        SSD1306_SETCONTRAST,                // 0x81
        0x8F };
    ssd1306_sendCommandPtr(init4a, sizeof(init4a));

    ssd1306_sendCommand(SSD1306_SETPRECHARGE); // 0xd9
    ssd1306_sendCommand(/* (vccstate == SSD1306_EXTERNALVCC) ? 0x22 :*/ 0xF1);
    static const uint8_t init5[] = {
        SSD1306_SETVCOMDETECT,               // 0xDB
        0x40,
        SSD1306_DISPLAYALLON_RESUME,         // 0xA4
        SSD1306_NORMALDISPLAY,               // 0xA6
        SSD1306_DEACTIVATE_SCROLL,
        SSD1306_DISPLAYON };                 // Main screen turn on
    ssd1306_sendCommandPtr(init5, sizeof(init5));



//   ssd1306_sendCommand(0xAE);            //display off
//   ssd1306_sendCommand(0xA6);            //Set Normal Display (default)
//   ssd1306_sendCommand(0xAE);            //DISPLAYOFF
//   ssd1306_sendCommand(0xD5);            //SETDISPLAYCLOCKDIV
//   ssd1306_sendCommand(0x80);            // the suggested ratio 0x80
//   ssd1306_sendCommand(0xA8);            //SSD1306_SETMULTIPLEX
//   ssd1306_sendCommand(0x3F);
//   ssd1306_sendCommand(0xD3);            //SETDISPLAYOFFSET
//   ssd1306_sendCommand(0x0);             //no offset
//   ssd1306_sendCommand(0x40|0x0);        //SETSTARTLINE
//   ssd1306_sendCommand(0x8D);            //CHARGEPUMP
//   ssd1306_sendCommand(0x14);
//   ssd1306_sendCommand(0x20);            //MEMORYMODE
//   ssd1306_sendCommand(0x00);            //0x0 act like ks0108
//   ssd1306_sendCommand(0xA1);            //SEGREMAP   Mirror screen horizontally (A0)
//   ssd1306_sendCommand(0xC8);            //COMSCANDEC Rotate screen vertically (C0)

// //   ssd1306_sendCommand(0xDA);            //0xDA
// //   ssd1306_sendCommand(0x12);            //COMSCANDEC

//   ssd1306_sendCommand(0xDA);            //0xDA
//   ssd1306_sendCommand(0x02);            //COMSCANDEC

// //   ssd1306_sendCommand(0x81);            //SETCONTRAST
// //   ssd1306_sendCommand(0xCF);            //

//   ssd1306_sendCommand(0x81);            //SETCONTRAST
//   ssd1306_sendCommand(0x8F);            //

//   ssd1306_sendCommand(0xd9);            //SETPRECHARGE 
//   ssd1306_sendCommand(0xF1);
//   ssd1306_sendCommand(0xDB);            //SETVCOMDETECT                
//   ssd1306_sendCommand(0x40);
//   ssd1306_sendCommand(0xA4);            //DISPLAYALLON_RESUME        
//   ssd1306_sendCommand(0xA6);            //NORMALDISPLAY             

//   ssd1306_clearDisplay();

//   ssd1306_sendCommand(0x2E);            //Stop scroll
//   ssd1306_sendCommand(0x20);            //Set Memory Addressing Mode
//   ssd1306_sendCommand(0x00);            //Set Memory Addressing Mode ab Horizontal addressing mode
// //   setFont(font8x8);



    // sendCommand(0xB0 + row);                          //set page address
    // sendCommand(0x00 + (m_font_width*col & 0x0F));    //set column lower addr
    // sendCommand(0x10 + ((m_font_width*col>>4)&0x0F)); //set column higher addr



    // ssd1306_sendCommand(0xB0 + 0);                          //set page address
    // ssd1306_sendCommand(0x00 + 0);    //set column lower addr
    // ssd1306_sendCommand(0x10 + 0); //set column higher addr

    // for(int i=0; i<128*32/8; i++) {
    //     ssd1306_sendData(0x0);
    // }

    // ssd1306_sendCommand(0xB0 + 0);                          //set page address
    // ssd1306_sendCommand(0x00 + 0);    //set column lower addr
    // ssd1306_sendCommand(0x10 + 0); //set column higher addr
    // ssd1306_sendData(0x55);
    // ssd1306_sendData(0xAA);

    // ssd1306_sendCommand(0xB0 + 0);                          //set page address
    // ssd1306_sendCommand(0x00 + 0);    //set column lower addr
    // ssd1306_sendCommand(0x10 + 0); //set column higher addr

    for(int i=0; i<128*32/8; i++) {
        ssd1306_buffer[i] = 0; //  (rand() + i) & 255;
    }

    // ssd1306_sendDataPtr((unsigned char *)&ssd1306_buffer, 128*32/8);

    // unsigned char buf1[] = { 
    //     SSD1306_PAGEADDR,
    //     0,                         // Page start address
    //     0xFF,                      // Page end (not really, but works here)
    //     SSD1306_COLUMNADDR,
    //     0 
    // };
    // ssd1306_sendCommandPtr((unsigned char *)&buf1, 5);
    // ssd1306_sendCommand(128 - 1); // Column end address

    // for(int i=0; i<40; i++) {
    //     unsigned char buf2[] = { 0xFF, 0x00, 0x55, 0xAA, 0xA5, 0x5A, 0xFF, 0x00 };
    //     ssd1306_sendDataPtr((unsigned char *)&buf2, 8);
    // }


    // ssd1306_sendData(0xA5);
    // ssd1306_sendData(0x5A);
    // ssd1306_sendData(0x55);
    // ssd1306_sendData(0xAA);
    // ssd1306_sendData(0xA5);
    // ssd1306_sendData(0x5A);
    // ssd1306_sendData(0x55);
    // ssd1306_sendData(0xAA);
    // ssd1306_sendData(0xA5);
    // ssd1306_sendData(0x5A);
    // ssd1306_sendData(0x55);
    // ssd1306_sendData(0xAA);
    // ssd1306_sendData(0xA5);
    // ssd1306_sendData(0x5A);
    // pgm_read_byte(&m_font[(ch-32)*m_font_width+m_font_offset+i])); 



    printf("done.\n");

    // volatile uint32_t cyclecounter1;
    // volatile uint32_t cyclecounter2;
    // volatile uint32_t cyclecountercount;

    // ESP_LOGI(TAG, "Compiletime WS2812 Timings");
    // ESP_LOGI(TAG, "0: %u + %u cycles", CYCLES2_800_T0H, CYCLES2_800_T0L);
    // ESP_LOGI(TAG, "1: %u + %u cycles", CYCLES2_800_T1H, CYCLES2_800_T1L);

    // taskENTER_CRITICAL();

    // cyclecounter1 = _getCycleCount();
    // for(volatile int i=100; i>=0; i--) {
    //     cyclecounter2 = _getCycleCount();
    // }

    // taskEXIT_CRITICAL();

    // ESP_LOGI(TAG, "WS2812 Bitbang init");
    // ESP_LOGI(TAG, "Cycle counter 1: %u", cyclecounter1);
    // ESP_LOGI(TAG, "Cycle counter 2: %u", cyclecounter2);

    // cyclecountercount = (cyclecounter2 - cyclecounter1) / 100;

    // ESP_LOGI(TAG, "Cycles needed for measuring cycle counter: %u", cyclecountercount);



    // taskENTER_CRITICAL();

    // cyclecounter1 = _getCycleCount();
    // for(volatile int i=10000; i>=0; i--) {
    //     __asm__ __volatile__("nop");
    // }
    // cyclecounter2 = _getCycleCount();

    // taskEXIT_CRITICAL();


    // ESP_LOGI(TAG, "Nop cycle counter 1: %u", cyclecounter1);
    // ESP_LOGI(TAG, "Nop cycle counter 2: %u", cyclecounter2);


}

// static portMUX_TYPE bb_mux = portMUX_INITIALIZER_UNLOCKED;


void display_update(uint8_t *pixels) {

// void ws2812_bitbang_update(uint8_t gpio_num, uint8_t *rgbs, size_t numpixels)
// gpio_set_direction(gpio_num, GPIO_MODE_OUTPUT);

// ws2812bitbang_gpiomask = (1 << gpio_num);

// taskENTER_CRITICAL(&bb_mux);

// bitbang_send_pixels_800_2(rgbs, rgbs + numpixels * 3, gpio_num);

// taskEXIT_CRITICAL(&bb_mux);

    // ssd1306_sendDataPtr((unsigned char *)&ssd1306_buffer, 128*32/8);

    unsigned char buf1[] = { 
        SSD1306_PAGEADDR,
        0,                         // Page start address
        0xFF,                      // Page end (not really, but works here)
        SSD1306_COLUMNADDR,
        0 };
    ssd1306_sendCommandPtr((unsigned char *)&buf1, 5);
    ssd1306_sendCommand(128 - 1); // Column end address

    for(int i=0; i<128*32/8; i++) {
        ssd1306_buffer[i] = (rand() + i) & 255;
    }


    ssd1306_buffer[0] = font8x8[48 * 8 + 0];
    ssd1306_buffer[1] = font8x8[48 * 8 + 1];
    ssd1306_buffer[2] = font8x8[48 * 8 + 2];
    ssd1306_buffer[3] = font8x8[48 * 8 + 3];
    ssd1306_buffer[4] = font8x8[48 * 8 + 4];
    ssd1306_buffer[5] = font8x8[48 * 8 + 5];
    ssd1306_buffer[6] = font8x8[48 * 8 + 6];
    ssd1306_buffer[7] = font8x8[48 * 8 + 7];

    ssd1306_sendDataPtr((unsigned char *)&ssd1306_buffer, 128 * 4);


    // int ret;
    // i2c_cmd_handle_t cmd = i2c_cmd_link_create();
    // i2c_master_start(cmd);
    // i2c_master_write_byte(cmd, BH1750_SENSOR_ADDR << 1 | WRITE_BIT, ACK_CHECK_EN);
    // i2c_master_write_byte(cmd, BH1750_CMD_START, ACK_CHECK_EN);
    // i2c_master_stop(cmd);
    // ret = i2c_master_cmd_begin(i2c_num, cmd, 1000 / portTICK_RATE_MS);
    // i2c_cmd_link_delete(cmd);
    // if (ret != ESP_OK) {
    //     return ret;
    // }
    // vTaskDelay(30 / portTICK_RATE_MS);
    // cmd = i2c_cmd_link_create();
    // i2c_master_start(cmd);
    // i2c_master_write_byte(cmd, BH1750_SENSOR_ADDR << 1 | READ_BIT, ACK_CHECK_EN);
    // i2c_master_read_byte(cmd, data_h, ACK_VAL);
    // i2c_master_read_byte(cmd, data_l, NACK_VAL);
    // i2c_master_stop(cmd);
    // ret = i2c_master_cmd_begin(i2c_num, cmd, 1000 / portTICK_RATE_MS);
    // i2c_cmd_link_delete(cmd);
    // return ret;

}

