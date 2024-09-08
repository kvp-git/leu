
// decoder address (1..126 = 0x01..0x7e)
#define ADDRESS  (0x43)
// decoder type
#define DECODER_TYPE DECODER_PULSE

enum DECODER_TYPES
{
  DECODER_SIGNAL = 1,
  DECODER_MOTOR = 2,
  DECODER_SENSOR = 3,
  DECODER_PULSE = 4,
};

#define ADDR_ALL (0x7f)

enum COMMANDS
{
  CMD_INFO = 0x00,
  CMD_SIGNAL_SET = 0x01,
  CMD_MOTOR_SET = 0x02,
  CMD_MOTOR_PULSE = 0x03,
  CMD_SENSOR_GET = 0x04,
  CMD_STOP_ALL = 0x7f,
};

enum STATES
{
  STATE_IDLE,
  STATE_ADDRESS,
  STATE_COMMAND,
  STATE_INFO,
  STATE_SIGNAL_SET,
  STATE_MOTOR_SET,
  STATE_PULSE_SET,
  STATE_SENSOR_GET,
  STATE_STOP_ALL,
};

#define SIGNAL_BLINK_MSEC (500)
#define SENSOR_POLL_MSEC (50)

#define FLAG_RESET  (1)
#define FLAG_MOTOR0 (2)
#define FLAG_MOTOR1 (4)
#define FLAG_MOTOR2 (8)
#define FLAG_MOTOR3 (16)
#define FLAG_ERROR  (64)

#define SIGNAL_NUM (16)
#define SENSOR_NUM (4)
#define MOTOR_NUM  (3)
#define PULSE_NUM  (4)

int state = STATE_IDLE;
unsigned long ts0 = 0;
unsigned statusFlags = FLAG_RESET;
uint8_t pktBuf[128];
unsigned pktCnt = 0;

uint16_t signals[2] = {0x5555, 0xaaaa};
unsigned signalsCnt = 0;
uint8_t motors[MOTOR_NUM] = {};
uint8_t sensors[SENSOR_NUM * 3] = {};
unsigned sensorCnt = 0;
int8_t pulseDir[PULSE_NUM] = {};
bool   pulseAct[PULSE_NUM] = {};
unsigned long pulseStart[PULSE_NUM] = {};
unsigned long pulseTime[PULSE_NUM] = {};


unsigned long timeDiff(unsigned long t0, unsigned long t1)
{
  if (t0 < t1)
    return (t1 - t0);
  return ((((unsigned long long)t1) + 0x100000000ul) - (unsigned long long)t0);
}

#define PIN_TXE (13)

int signalPins[SIGNAL_NUM] = { 2, 3, 4, 5, 6, 7, 8, 9, 10, 11, 12, A0, A1, A2, A3 };
int pwmPins[MOTOR_NUM * 2] = { 3, 5, 6, 9, 10, 11 };
int analogPins[SENSOR_NUM] = { A0, A1, A2, A3 };
int pulsePins[PULSE_NUM * 2] = { 3, 5, 6, 9, 10, 11, A1, A2 };

void setup()
{
  Serial.begin(115200);
  pinMode(PIN_TXE, OUTPUT);
  digitalWrite(PIN_TXE, LOW);
  switch (DECODER_TYPE)
  {
    case DECODER_SIGNAL:
    {
      for (int t = 0; t < SIGNAL_NUM; t++)
      {
        pinMode(signalPins[t], OUTPUT);
        digitalWrite(signalPins[t], LOW);
      }
      break;
    }
    case DECODER_MOTOR:
    {
      for (int t = 0; t < MOTOR_NUM; t++)
      {
        pinMode(pwmPins[t], OUTPUT);
        digitalWrite(pwmPins[t], LOW);
      }
/*
timer 0 (controls pin 13, 4);
timer 1 (controls pin 12, 11);
timer 2 (controls pin 10, 9);
timer 3 (controls pin 5, 3, 2);
timer 4 (controls pin 8, 7, 6);

prescaler = 1 ---> PWM frequency is 31000 Hz
prescaler = 2 ---> PWM frequency is 4000 Hz
prescaler = 3 ---> PWM frequency is 490 Hz (default value)
prescaler = 4 ---> PWM frequency is 120 Hz
prescaler = 5 ---> PWM frequency is 30 Hz
prescaler = 6 ---> PWM frequency is <20 Hz
(prescalers equal t 0 or 7 are useless).

Those prescaler values are good for all timers (TCCR1B, TCCR2B, TCCR3B, TCCR4B) except for timer 0 (TCCR0B).
In this case the values are:

prescaler = 1 ---> PWM frequency is 62000 Hz
prescaler = 2 ---> PWM frequency is 7800 Hz
prescaler = 3 ---> PWM frequency is 980 Hz (default value)
prescaler = 4 ---> PWM frequency is 250 Hz
prescaler = 5 ---> PWM frequency is 60 Hz
prescaler = 6 ---> PWM frequency is <20 Hz

Note that timer 0 is the one on which rely all time functions in Arduino: i.e., if you change this timer, 
function like delay() or millis() will continue to work but at a different timescale (quicker or slower).

The value of variable TCCRnB, where 'n' is the number of register.
The TCCRnB is a 8 bit number. The first three bits (from right to left!) are called CS02, CS01, CS00, and 
they are the bits we have to change.
*/
      TCCR0B = (TCCR0B & (~7)) | 2;
      TCCR1B = (TCCR1B & (~7)) | 2;
      TCCR2B = (TCCR2B & (~7)) | 2;
      /*
      uint8_t pkt[3] = {16, 16, 16};
      motorSet(pkt);
      */
      break;
    }
    case DECODER_SENSOR:
    {
      for (int t = 0; t < SENSOR_NUM; t++)
        pinMode(analogPins[t], INPUT_PULLUP);
      break;
    }
    case DECODER_PULSE:
    {
      for (int t = 0; t < PULSE_NUM * 2; t++)
      {
        pinMode(pulsePins[t], OUTPUT);
        digitalWrite(pulsePins[t], LOW);
      }
      
      break;
    }
  }
  /*
  digitalWrite(PIN_TXE, HIGH);
  uint8_t pkt[2] = {0x55, 0xaa};
  Serial.write(pkt, 2);
  Serial.flush();
  digitalWrite(PIN_TXE, LOW);
  */
}

void signalSet(const uint8_t* data)
{
  signals[0] = ((unsigned)data[0]) | (((unsigned)data[1]) << 7) | ((((unsigned)data[2]) & 3) << 14);
  signals[1] = ((((unsigned)data[2]) >> 2) & 31) | (((unsigned)data[3]) << 5) | ((((unsigned)data[4]) & 15) << 12);
}

void motorSet(const uint8_t* data)
{
  for (int t = 0; t < MOTOR_NUM; t++)
  {
    uint8_t v = data[t];
    if (v == motors[t])
      continue;
    motors[t] = v;
    uint8_t s = (v & 63) << 2;
    if (v == 0)
    {
      analogWrite(pwmPins[t * 2], 0);
      analogWrite(pwmPins[t * 2 + 1], 0);
      //digitalWrite(pwmPins[t * 2], LOW);
      //digitalWrite(pwmPins[t * 2 + 1], LOW); 
    } else if ((v & 64) != 0)
    {
      analogWrite(pwmPins[t * 2], 0);
      analogWrite(pwmPins[t * 2 + 1], 0);
      analogWrite(pwmPins[t * 2 + 1], s);
    } else
    {
      analogWrite(pwmPins[t * 2], 0);
      analogWrite(pwmPins[t * 2 + 1], 0);
      analogWrite(pwmPins[t * 2], s);
    }
    statusFlags |= (1 << (t + 1));
  }
}

void pulseOut(int num, int value)
{
  switch (value)
  {
    case -1: // reverse
      digitalWrite(pulsePins[num * 2], HIGH);
      digitalWrite(pulsePins[num * 2 + 1], LOW);
      pulseAct[num] = true;
      break;
    case 1: // forward
      digitalWrite(pulsePins[num * 2], LOW);
      digitalWrite(pulsePins[num * 2 + 1], HIGH);
      pulseAct[num] = true;
      break;
    case -2: // reverse cont.
      digitalWrite(pulsePins[num * 2], HIGH);
      digitalWrite(pulsePins[num * 2 + 1], LOW);
      pulseAct[num] = false;
      break;
    case 2: // forward cont.
      digitalWrite(pulsePins[num * 2], LOW);
      digitalWrite(pulsePins[num * 2 + 1], HIGH);
      pulseAct[num] = false;
      break;
    case 0: // off
    default:
      digitalWrite(pulsePins[num * 2], HIGH);     // L293D VCC1 current mitigation (brake mode for modern drivers)
      digitalWrite(pulsePins[num * 2 + 1], HIGH); // change this to LOW, LOW for modern drivers (freewheeling)
      pulseAct[num] = false;
      break;
  }
}

// _wwwwwww _DWWWWWW -> D, WWWWWWwwwwwwww
void motorPulse(const uint8_t* data)
{
  unsigned long len;
  int dir;
  unsigned long ts = millis();
  for (int t = 0; t < PULSE_NUM; t++)
  {
    len = ((unsigned long)(data[t * 2] & 127)) | (((unsigned long)(data[t * 2 + 1] & 63)) << 7);
    dir = ((data[t * 2 + 1] & 64) != 0) ? -1 : 1;
    if (len == 0)
      dir = 0;
    if ((dir != pulseDir[t]) || (len != pulseTime[t]))
    {
      pulseStart[t] = ts;
      pulseTime[t] = len;
      pulseDir[t] = dir;
      pulseOut(t, dir * ((len == 8191) ? 2 : 1));
    }
  }
}

void signalOut(unsigned aspects)
{
  for (size_t t = 0; t < SIGNAL_NUM; t++)
    digitalWrite(signalPins[t], ((aspects & (1 << t)) == 0) ? LOW : HIGH);
}

void pktSend(uint8_t* pkt, size_t len)
{
  if (DECODER_TYPE == DECODER_MOTOR)
    delay(10*7); // TODO!!! clean this up
  else
    delay(10);
  unsigned chk = 0xff;
  if (len < 1)
    return;
  for (size_t t = 0; t < (len - 1); t++)
    chk += pkt[t];
  pkt[len - 1] = chk & 127;
  digitalWrite(PIN_TXE, HIGH);
  Serial.write(pkt, len);
  Serial.flush();
  digitalWrite(PIN_TXE, LOW);
}

bool cmdIn(const uint8_t* pkt, size_t len)
{
  unsigned int chk = 0xff;
  if (len < 3)
    return false;
  for (size_t t = 0; t < (len - 1); t++)
    chk += pkt[t];
  chk &= 127;
  if (pkt[len - 1] != chk)
    return false;
  bool allFlag = ((pkt[0] & 127) == ADDR_ALL);
  statusFlags &= ~FLAG_RESET;
  // check command
  switch (pkt[1])
  {
    case CMD_INFO:
    {
      if (allFlag)
        break;
      uint8_t res[3];
      res[0] = DECODER_TYPE;
      res[1] = statusFlags;
      pktSend(res, 3);
      break;
    }
    case CMD_SIGNAL_SET:
    {
      signalSet(pkt + 2);
      if (allFlag)
        break;
      uint8_t res[2];
      res[0] = statusFlags;
      pktSend(res, 2);
      break;
    }
    case CMD_MOTOR_SET:
    {
      motorSet(pkt + 2);
      if (allFlag)
        break;
      uint8_t res[2];
      res[0] = statusFlags;
      pktSend(res, 2);
      break;
    }
    case CMD_MOTOR_PULSE:
    {
      motorPulse(pkt + 2);
      if (allFlag)
        break;
      uint8_t res[2];
      res[0] = statusFlags;
      pktSend(res, 2);
      break;
    }
    case CMD_SENSOR_GET:
    {
      if (allFlag)
        break;
      uint8_t res[14];
      res[0] = statusFlags;
      for (size_t t = 0; t < (SENSOR_NUM * 3); t++)
        res[1 + t] = sensors[t];
      for (size_t t = 0; t < SENSOR_NUM; t++) // reset min max
      {
        sensors[t * 3 + 1] = sensors[t * 3];
        sensors[t * 3 + 2] = sensors[t * 3];
      }
      pktSend(res, 14);
      break;
    }
    case CMD_STOP_ALL:
      switch (DECODER_TYPE)
      {
        case DECODER_SIGNAL:
          signals[0] = 0;
          signals[1] = 0;
          for (int t = 0; t < SIGNAL_NUM; t++)
            digitalWrite(signalPins[t], LOW);
          break;
        case DECODER_MOTOR:
          for (int t = 0; t < MOTOR_NUM; t++)
            digitalWrite(pwmPins[t], LOW);
          break;
        case DECODER_SENSOR:
          break;
        case DECODER_PULSE:
          for (int t = 0; t < PULSE_NUM * 2; t++)
            digitalWrite(pulsePins[t], LOW);
          break;
      }
      statusFlags = FLAG_RESET;
      if (allFlag)
        break;
      uint8_t res[2];
      res[0] = statusFlags;
      pktSend(res, 2);
      break;
  }
  return true;
}

void dataIn(uint8_t data)
{
  // check for start of packet bit
  if ((data & 128) != 0)
  {
      pktCnt = 0;
      state = STATE_ADDRESS;
  }
  // check for idle state (ignore traffic until start of packet)
  if (state == STATE_IDLE)
    return;
  // check for pkt rx buffer full
  if (pktCnt < sizeof(pktBuf))
    pktBuf[pktCnt++] = data;
  switch (state)
  {
    case STATE_ADDRESS:
      if ((data == (ADDRESS | 128)) || (data == (ADDR_ALL | 128)))
        state = STATE_COMMAND;
      else
        state = STATE_IDLE;     
      return;
    case STATE_COMMAND:
      switch (data)
      {
        case CMD_INFO:
          state = STATE_INFO;
          return;
        case CMD_SIGNAL_SET:
          if (DECODER_TYPE == DECODER_SIGNAL)
            state = STATE_SIGNAL_SET;
          else
            state = STATE_IDLE;
          return;
        case CMD_MOTOR_SET:
          if (DECODER_TYPE == DECODER_MOTOR)
            state = STATE_MOTOR_SET;
          else
            state = STATE_IDLE;
          return;
        case CMD_MOTOR_PULSE:
          if (DECODER_TYPE == DECODER_PULSE)
            state = STATE_PULSE_SET;
          else
            state = STATE_IDLE;
          return;
        case CMD_SENSOR_GET:
          if (DECODER_TYPE == DECODER_SENSOR)
            state = STATE_SENSOR_GET;
          else
            state = STATE_IDLE;
          return;
        case CMD_STOP_ALL:
          state = STATE_STOP_ALL;
          return;
        default:
          state = STATE_IDLE;
          return;
      }
      return;
    case STATE_INFO:
      cmdIn(pktBuf, pktCnt);
      state = STATE_IDLE;
      return;
    case STATE_SIGNAL_SET:
      if (pktCnt == 8)
      {
        cmdIn(pktBuf, pktCnt);
        state = STATE_IDLE;
      }
      return;
    case STATE_MOTOR_SET:
      if (pktCnt == 7)
      {
        cmdIn(pktBuf, pktCnt);
        state = STATE_IDLE;
      }
      return;
    case STATE_PULSE_SET:
      if (pktCnt == 11)
      {
        cmdIn(pktBuf, pktCnt);
        state = STATE_IDLE;
      }
      return;
    case STATE_SENSOR_GET:
      cmdIn(pktBuf, pktCnt);
      state = STATE_IDLE;
      return;
    case STATE_STOP_ALL:
      cmdIn(pktBuf, pktCnt);
      state = STATE_IDLE;
      return;
    default:
      state = STATE_IDLE;
      return;
  }
}

void loop()
{
  int c = Serial.read();
  if (c >= 0)
    dataIn(c);
  unsigned long ts = millis();
  switch (DECODER_TYPE)
  {
    case DECODER_SIGNAL:
      if (timeDiff(ts0, ts) >= SIGNAL_BLINK_MSEC)
      {
        signalOut(signals[signalsCnt]);
        signalsCnt = (signalsCnt + 1) & 1;
        ts0 = ts; 
      }
      break;
    case DECODER_MOTOR:
      /* TODO!!!
      digitalWrite(PIN_TXE, HIGH);
      Serial.print ("HELLO ");
      Serial.println(ts);
      digitalWrite(PIN_TXE, LOW);
      delay(1000);
      Serial.println(TCCR0B);
      Serial.println(TCCR1B);
      Serial.println(TCCR2B);
      Serial.println("");
      */
      break;
    case DECODER_SENSOR:
      if (timeDiff(ts0, ts) >= SENSOR_POLL_MSEC)
      {
        int v = analogRead(analogPins[sensorCnt >> 1]);
        if ((sensorCnt & 1) == 1) // save every 2nd read (input switch lag workaround)
        {
          // 4095 -> 127
          uint8_t data = (v >> 5) & 127;
          unsigned idx = (sensorCnt >> 1) * 3;
          statusFlags |= (1 << (idx + 1));
          if ((statusFlags & (2 | 4 | 8 | 16)) == (2 | 4 | 8 | 16))
            statusFlags = ~FLAG_RESET;
          sensors[idx] = data;
          if (sensors[idx + 1] > data)
            sensors[idx + 1] = data;
          if (sensors[idx + 2] < data)
            sensors[idx + 2] = data;
        }
        sensorCnt = (sensorCnt + 1) & 7;
        ts0 = ts;
      }
      break;
    case DECODER_PULSE:
      for (int t = 0; t < PULSE_NUM; t++)
      {
        if (!pulseAct[t])
          continue;
        if (timeDiff(pulseStart[t], ts) >= pulseTime[t])
          pulseOut(t, 0);
      }
      break;
  }
}
