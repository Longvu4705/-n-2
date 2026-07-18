#include <mega16.h>
#include <delay.h>
#include <stdio.h>

/* =========================
   CAU HINH LCD THEO MACH
   RS -> PD6
   RW -> PD5
   E  -> PD7
   D4 -> PC4
   D5 -> PC5
   D6 -> PC6
   D7 -> PC7
   ========================= */
#define LCD_RS PORTD.6
#define LCD_RW PORTD.5
#define LCD_E  PORTD.7

#define LCD_RS_DDR DDRD.6
#define LCD_RW_DDR DDRD.5
#define LCD_E_DDR  DDRD.7

/* =========================
   CAU HINH DS18B20
   DQ -> PA7
   Co dien tro keo len 4.7k
   ========================= */

#define OW_DQ_PORT PORTA.7
#define OW_DQ_DDR  DDRA.7
#define OW_DQ_PIN  PINA.7

/* =========================
   HAM LCD 4 BIT
   ========================= */

void lcd_enable(void)
{
    LCD_E = 1;
    delay_us(2);
    LCD_E = 0;
    delay_us(100);
}

void lcd_send_nibble(unsigned char data)
{
    PORTC = (PORTC & 0x0F) | ((data & 0x0F) << 4);
    lcd_enable();
}

void lcd_send_byte(unsigned char data, unsigned char rs)
{
    LCD_RW = 0;      // LCD luon o che do ghi
    LCD_RS = rs;

    // Gui 4 bit cao
    lcd_send_nibble(data >> 4);

    // Gui 4 bit thap
    lcd_send_nibble(data & 0x0F);
}

void lcd_cmd(unsigned char cmd)
{
    lcd_send_byte(cmd, 0);

    if (cmd == 0x01 || cmd == 0x02)
        delay_ms(2);
    else
        delay_us(100);
}

void lcd_data(unsigned char data)
{
    lcd_send_byte(data, 1);
}

void lcd_init_custom(void)
{
    // PC4-PC7 la output
    DDRC |= 0xF0;

    // PD5, PD6, PD7 la output
    LCD_RS_DDR = 1;
    LCD_RW_DDR = 1;
    LCD_E_DDR  = 1;

    LCD_RS = 0;
    LCD_RW = 0;
    LCD_E  = 0;

    delay_ms(20);

    // Khoi tao LCD 4-bit
    lcd_send_nibble(0x03);
    delay_ms(5);

    lcd_send_nibble(0x03);
    delay_us(150);

    lcd_send_nibble(0x03);
    delay_us(150);

    lcd_send_nibble(0x02);   // chuyen sang che do 4-bit

    lcd_cmd(0x28);           // 4-bit, 2 dong, font 5x8
    lcd_cmd(0x0C);           // bat hien thi, tat con tro
    lcd_cmd(0x06);           // tu dong tang vi tri con tro
    lcd_cmd(0x01);           // xoa man hinh
}

void lcd_gotoxy(unsigned char x, unsigned char y)
{
    if (y == 0)
        lcd_cmd(0x80 + x);
    else
        lcd_cmd(0xC0 + x);
}

void lcd_puts_custom(char *s)
{
    while (*s)
    {
        lcd_data(*s);
        s++;
    }
}

/* =========================
   HAM 1-WIRE CHO DS18B20
   ========================= */

void ow_low(void)
{
    OW_DQ_PORT = 0;
    OW_DQ_DDR = 1;      // PA7 output, keo bus xuong 0
}

void ow_release(void)
{
    OW_DQ_DDR = 0;      // PA7 input, nha bus
    OW_DQ_PORT = 1;     // Bat pull-up noi bo
}

unsigned char ow_read_pin(void)
{
    return OW_DQ_PIN;
}

unsigned char ow_reset(void)
{
    unsigned char presence;

    ow_low();
    delay_us(500);

    ow_release();
    delay_us(70);

    presence = !ow_read_pin();

    delay_us(410);

    return presence;
}

void ow_write_bit(unsigned char bit_value)
{
    if (bit_value)
    {
        ow_low();
        delay_us(6);
        ow_release();
        delay_us(64);
    }
    else
    {
        ow_low();
        delay_us(60);
        ow_release();
        delay_us(10);
    }
}

unsigned char ow_read_bit(void)
{
    unsigned char bit_value;

    ow_low();
    delay_us(6);

    ow_release();
    delay_us(9);

    bit_value = ow_read_pin();

    delay_us(55);

    return bit_value;
}

void ow_write_byte(unsigned char data)
{
    unsigned char i;

    for (i = 0; i < 8; i++)
    {
        ow_write_bit(data & 0x01);
        data >>= 1;
    }
}

unsigned char ow_read_byte(void)
{
    unsigned char i;
    unsigned char data = 0;

    for (i = 0; i < 8; i++)
    {
        if (ow_read_bit())
            data |= (1 << i);
    }

    return data;
}

/* =========================
   DOC NHIET DO DS18B20
   Tra ve nhiet do x10
   Vi du: 27.0 do C -> 270
   ========================= */

signed int ds18b20_read_temp_x10(void)
{
    unsigned char temp_lsb;
    unsigned char temp_msb;
    signed int raw;
    signed long temp_x10;

    if (!ow_reset())
    {
        return 9999;    // loi khong thay cam bien
    }

    ow_write_byte(0xCC);    // Skip ROM
    ow_write_byte(0x44);    // Convert T

    delay_ms(750);          // cho DS18B20 do xong

    if (!ow_reset())
    {
        return 9999;
    }

    ow_write_byte(0xCC);    // Skip ROM
    ow_write_byte(0xBE);    // Read Scratchpad

    temp_lsb = ow_read_byte();
    temp_msb = ow_read_byte();

    raw = ((signed int)temp_msb << 8) | temp_lsb;

    // DS18B20 mac dinh 12-bit: nhiet do = raw / 16
    // temp_x10 = nhiet do * 10 = raw * 10 / 16
    temp_x10 = ((signed long)raw * 10) / 16;

    return (signed int)temp_x10;
}

void lcd_show_temperature(signed int temp_x10)
{
    char buffer[10];
    signed int temp_abs;

    lcd_gotoxy(0, 0);
    lcd_puts_custom("Nhiet do:       ");

    lcd_gotoxy(4, 1);

    if (temp_x10 == 9999)
    {
        lcd_puts_custom("Sensor Error");
        return;
    }

    if (temp_x10 < 0)
    {
        lcd_data('-');
        temp_abs = -temp_x10;
    }
    else
    {
        temp_abs = temp_x10;
    }

    sprintf(buffer, "%d.%d", temp_abs / 10, temp_abs % 10);

    lcd_puts_custom(buffer);
    lcd_data(0xDF);      // ky tu do tren LCD1602
    lcd_data('C');
    lcd_puts_custom("     ");
}

/* =========================
   CHUONG TRINH CHINH
   ========================= */

void main(void)
{
    signed int temperature;

    // Tat JTAG vi LCD dung PC4, PC5
    // Neu khong tat, PC4/PC5 co the khong dieu khien LCD dung
    MCUCSR |= 0x80;
    MCUCSR |= 0x80;
    
    DDRA = 0x00;
    PORTA = 0x00;

    OW_DQ_DDR = 0;
    OW_DQ_PORT = 1;

    lcd_init_custom();

    lcd_gotoxy(0, 0);
    lcd_puts_custom("ATmega16");
    lcd_gotoxy(0, 1);
    lcd_puts_custom("DS18B20 Ready");
    delay_ms(1000);

    lcd_cmd(0x01);

    while (1)
    {
        temperature = ds18b20_read_temp_x10();

        lcd_show_temperature(temperature);

        delay_ms(500);
    }
}