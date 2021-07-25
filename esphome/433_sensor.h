char have_data;
char have_data_string;
std::string my_data;
float temp;
float hum;


class MyCustomSensor : public PollingComponent {
  public:


    Sensor *sensor1 = new Sensor();
    Sensor *sensor2 = new Sensor();
//Based on code from: https://github.com/lucsmall/BetterWH2
//converted from polling to interupt and adjusted to compile with esphome by Swedude




    // 1 is indicated by 500uS pulse
    // wh2_accept from 2 = 400us to 3 = 600us
#define IS_HI_PULSE(interval)   (interval >= 350 && interval <= 750)
    // 0 is indicated by ~1500us pulse
    // wh2_accept from 7 = 1400us to 8 = 1600us
#define IS_LOW_PULSE(interval)  (interval >= 1200 && interval <= 1700)
    // worst case packet length
    // 6 bytes x 8 bits =48
#define IDLE_HAS_TIMED_OUT(interval) (interval > 1150)
    // our expected pulse should arrive after 1ms
    // we'll wh2_accept it if it arrives after
    // 4 x 200us = 800us
#define IDLE_PERIOD_DONE(interval) (interval >= 800)

#define GOT_PULSE 0x01
#define LOGIC_HI  0x02

    static void ICACHE_RAM_ATTR ext_int_1()
    {

      static uint16_t pulse;
      static byte wh2_flags;
      static byte wh2_accept_flag;
      static byte wh2_packet_state;

      static byte wh2_packet[5];
      static byte wh2_calculated_crc;
      static boolean wh2_valid;
      static unsigned long last;
      static unsigned long micros_now;
      static byte sampling_state = 0;
      static byte packet_no, bit_no, history;
      micros_now = micros();
      pulse = micros_now - last;
      last = micros_now;



      switch (sampling_state) {
        case 0: // waiting

          if (IS_HI_PULSE(pulse)) {
            wh2_flags = GOT_PULSE | LOGIC_HI;
            sampling_state = 1;

          } else if (IS_LOW_PULSE(pulse)) {
            wh2_flags = GOT_PULSE; // logic low

          } else {
            sampling_state = 0;
          }
          break;
        case 1: // observe 1ms of idle time

          if (IDLE_HAS_TIMED_OUT(pulse)) {
            sampling_state = 0;
            wh2_packet_state = 1;

          } else if (IDLE_PERIOD_DONE(pulse)) {
            sampling_state = 0;
          }
          else
          {
            sampling_state = 0;
            wh2_packet_state = 1;
          }
          break;
      }

      //----------------------------------------------------
      if (wh2_flags) {

        //----------------------------------------------------

        // acquire preamble
        if (wh2_packet_state == 1) {
          // shift history right and store new value
          history <<= 1;
          // store a 1 if required (right shift along will store a 0)
          if (wh2_flags & LOGIC_HI) {
            history |= 0x01;
          }

          // check if we have a valid start of frame
          // xxxxx110
          if ((history & B00000111) == B00000110) {
            // need to clear packet, and pulseers
            packet_no = 0;
            // start at 1 becuase only need to acquire 7 bits for first packet byte.
            bit_no = 1;
            wh2_packet[0] = wh2_packet[1] = wh2_packet[2] = wh2_packet[3] = wh2_packet[4] = 0;
            // we've acquired the preamble
            wh2_packet_state = 2;
            history = 0xFF;
          }
          wh2_accept_flag = false;
        }
        // acquire packet
        else if (wh2_packet_state == 2) {
          wh2_packet[packet_no] <<= 1;
          if (wh2_flags & LOGIC_HI) {
            wh2_packet[packet_no] |= 0x01;
          }
          bit_no ++;
          if (bit_no > 7) {
            bit_no = 0;
            packet_no ++;
          }
          if (packet_no > 4) {
            // start the sampling process from scratch
            wh2_packet_state = 1;

            wh2_accept_flag = true;
          }
        }
        else
        {
          wh2_accept_flag = false;
        }

        //---------------------------------------------

        if (wh2_accept_flag) {

          int Sensor_ID = (wh2_packet[0] << 4) + (wh2_packet[1] >> 4);
          int humidity = wh2_packet[3];
          int temperature = ((wh2_packet[1] & B00000111) << 8) + wh2_packet[2];
          // make negative
          if (wh2_packet[1] & B00001000) {
            temperature = -temperature;
          }
          uint8_t crc = 0;
          uint8_t len = 4;
          uint8_t *addr = wh2_packet;
          // Indicated changes are from reference CRC-8 function in OneWire library
          while (len--) {
            uint8_t inbyte = *addr++;
            for (uint8_t i = 8; i; i--) {
              uint8_t mix = (crc ^ inbyte) & 0x80; // changed from & 0x01
              crc <<= 1; // changed from right shift
              if (mix) crc ^= 0x31;// changed from 0x8C;
              inbyte <<= 1; // changed from right shift
            }
          }
          if (crc == wh2_packet[4])
          {
            wh2_valid = true;
          }
          else
          {
            wh2_valid = false;
          }
          /*
                Serial.print("| Sensor ID: ");
                Serial.print(Sensor_ID);
                Serial.print(" | humidity: ");
                Serial.print(humidity, DEC);//power
                Serial.print(" | temperature: ");
                Serial.print(temperature / 10.0);
                Serial.print("C | ");
                Serial.println((wh2_valid ? "OK " : "BAD" ));
          */


          if (Sensor_ID == 68 && wh2_valid == true)
          {
            extern char have_data;
            have_data = 1;
            extern float temp;
            temp = temperature / 10.0;
            extern float hum;
            hum = humidity;
          }

          extern std::string my_data;
          my_data = "| Sensor ID: ";
          my_data += to_string(Sensor_ID);
          my_data += " | humidity: ";
          my_data += to_string(humidity);
          my_data += " | temperature: ";
          my_data += to_string(temperature / 10.0);
          my_data += "C | ";
          my_data += (wh2_valid ? "OK " : "BAD" );
          extern char have_data_string;
          have_data_string = 1;


          wh2_accept_flag = false;
        }
        wh2_flags = 0x00;
      }
      //--------------------------------------------------------
    }



    MyCustomSensor() : PollingComponent(100) { }

    void setup() override 
	{
		
		
      Serial.end();//if you are using rx pin for reciever (3)
	  
      attachInterrupt(3, ext_int_1, CHANGE);
	  
	  
    }



    void update() override {
      // This is the actual sensor reading logic.
      extern std::string my_data;
      extern char have_data;
      extern float temp;
      extern float hum;

      if (have_data)
      {
        have_data = 0;

        sensor1->publish_state(temp);
        sensor2->publish_state(hum);
      }

    }
};




class Last_sensor_data : public PollingComponent, public TextSensor {
  public:

    // constructor
    Last_sensor_data() : PollingComponent(100) {}

    void setup() override {
      // This will be called by App.setup()
    }
    void update() override {
      extern std::string my_data;
      extern char have_data_string;
      if (have_data_string == 1)
      {
        publish_state(my_data);
	    have_data_string = 0;
      }

      // This will be called every "update_interval" milliseconds.
      // Publish state

    }
};



