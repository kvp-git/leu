
// decoder address (1..126)
#define ADDRESS  (0x42)
// decoder type
#define DECODER_TYPE DECODER_SIGNAL

enum DECODER_TYPES
{
  DECODER_SIGNAL = 1,
  DECODER_MOTOR = 2,
  DECODER_SENSOR = 3,
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
  STATE_MOTOR_PULSE,
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

int state = STATE_IDLE;
unsigned long ts0 = 0;
unsigned statusFlags = FLAG_RESET;
uint8_t pktBuf[128];
unsigned pktCnt = 0;

uint16_t signals[2] = {0x5555, 0xaaaa};
unsigned signalsCnt = 0;
int8_t motors[4] = {};
uint8_t sensors[4 * 3] = {};
unsigned sensorCnt = 0;

unsigned long timeDiff(unsigned long t0, unsigned long t1)
{
  if (t0 < t1)
    return (t1 - t0);
  return ((((unsigned long long)t1) + 0x100000000ul) - (unsigned long long)t0);
}

#define PIN_TXE (13)

int signalPins[16] = { 2, 3, 4, 5, 6, 7, 8, 9, 10, 11, 12, A0, A1, A2, A3 };
int pwmPins[6] = { 3, 5, 6, 9, 10, 11 };
int analogPins[4] = { A0, A1, A2, A3};

void setup()
{
  Serial.begin(115200);
  pinMode(PIN_TXE, OUTPUT);
  digitalWrite(PIN_TXE, LOW);
  switch (DECODER_TYPE)
  {
    case DECODER_SIGNAL:
      for (int t = 0; t < 16; t++)
      {
        pinMode(signalPins[t], OUTPUT);
        digitalWrite(signalPins[t], LOW);
      }
      break;
    case DECODER_MOTOR:
      for (int t = 0; t < 6; t++)
      {
        pinMode(pwmPins[t], OUTPUT);
        digitalWrite(pwmPins[t], LOW);
      }
      break;
    case DECODER_SENSOR:
      for (int t = 0; t < 4; t++)
      {
        pinMode(analogPins[t], INPUT);
      }
      break;
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
  for (int t = 0; t < 3; t++)
  {
    int8_t v = (int8_t)data[t];
    if (v == motors[t])
      continue;
    motors[t] = v;
    if (v < 0)
    {
      analogWrite(pwmPins[t * 2], 0);
      analogWrite(pwmPins[t * 2 + 1], 0);
      analogWrite(pwmPins[t * 2 + 1], v * -2);
    } else if (v > 0)
    {
      analogWrite(pwmPins[t * 2], 0);
      analogWrite(pwmPins[t * 2 + 1], 0);
      analogWrite(pwmPins[t * 2], v * 2);
    } else
    {
      analogWrite(pwmPins[t * 2], 0);
      analogWrite(pwmPins[t * 2 + 1], 0);
      //digitalWrite(pwmPins[t * 2], LOW);
      //digitalWrite(pwmPins[t * 2 + 1], LOW);
    }
    statusFlags |= (1 << (t + 1));
  }
}

void motorPulse(const uint8_t* data)
{
  // TODO!!!
}

void signalOut(unsigned aspects)
{
  for (size_t t = 0; t < 16; t++)
    digitalWrite(signalPins[t], ((aspects & (1 << t)) == 0) ? LOW : HIGH);
}

void pktSend(uint8_t* pkt, size_t len)
{
  delay(10); // TODO!!! check this
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
      for (size_t t = 0; t < (4 * 3); t++)
        res[1 + t] = sensors[t];
      for (size_t t = 0; t < 4; t++) // reset min max
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
          for (int t = 0; t < 16; t++)
            digitalWrite(signalPins[t], LOW);
          break;
        case DECODER_MOTOR:
          // TODO!!!
          for (int t = 0; t < 6; t++)
            digitalWrite(pwmPins[t], LOW);
          break;
        case DECODER_SENSOR:
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
          if (DECODER_TYPE == DECODER_MOTOR)
            state = STATE_MOTOR_PULSE;
          else
            state = STATE_IDLE;
          return;
        case CMD_SENSOR_GET:
          if (DECODER_TYPE == DECODER_MOTOR)
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
    case STATE_MOTOR_PULSE:
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
      // TODO!!!
      break;
    case DECODER_SENSOR:
      if (timeDiff(ts0, ts) >= SENSOR_POLL_MSEC)
      {
        int v = analogRead(analogPins[sensorCnt / 2]);
        if ((sensorCnt & 1) == 1) // save every 2nd read (input switch lag workaround)
        {
          // 4095 -> 127
          uint8_t data = v >> 5;
          unsigned idx = (sensorCnt >> 1) * 3;
          sensors[idx] = data;
          if (sensors[idx + 1] > data)
            sensors[idx + 1] = data;
          if (sensors[idx + 2] < data)
            sensors[idx + 2] = data;
        }
        sensorCnt = (sensorCnt + 1) & 7;
        ts0 = ts;
      }
      // TODO!!!
      break;
  }
 /*
  if (timeDiff(ts0, ts) > TICK)
  {
    switch (DEOCDER_TYPE)
    {
      case DECODER_SIGNAL:
        tickSignal();
        break;
      case DECODER_MOTOR:
        tickMotor();
        break;
      case DECODER_SENSOR:
        tickSensor();
        break;
    }
*/
}
