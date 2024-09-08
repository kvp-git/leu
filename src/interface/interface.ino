#define PIN_LED (13)
#define PIN_RS485ENA (14)

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

#define REPLY_WAIT_MSEC (500)

int blinkState = HIGH;

unsigned long timeDiff(unsigned long t0, unsigned long t1)
{
  if (t0 < t1)
    return (t1 - t0);
  return ((((unsigned long long)t1) + 0x100000000ul) - (unsigned long long)t0);
}

void dumpPkt(const char* tag, const uint8_t* pkt, size_t len)
{
  Serial.print(tag);
  for (size_t t = 0; t < len; t++)
  {
    Serial.print(" 0x");
    Serial.print((pkt[t] >> 4) & 15, HEX);
    Serial.print(pkt[t] & 15, HEX);
  }
  Serial.println("");
}

#define ERR_ERR (-1)
#define ERR_TX  (-2)
#define ERR_LEN (-3)
#define ERR_TO  (-4)
#define ERR_CHK (-5)

uint8_t rxBuf[32];

int rs485txrx(uint8_t* pkt, size_t len, uint8_t* res, size_t resLen)
{
  if (len < 3)
    return ERR_ERR;
  pkt[len - 1] = 0xff;
  for (size_t t = 0; t < (len - 1); t++)
    pkt[len - 1] += pkt[t];
  pkt[len - 1] &= 127;
  pkt[0] |= 128;
  dumpPkt("# TX", pkt, len);
  digitalWrite(PIN_RS485ENA, HIGH);
  digitalWrite(PIN_LED, blinkState);
  blinkState = ((blinkState == LOW) ? HIGH : LOW);
  Serial1.write(pkt, len);
  Serial1.flush();
  digitalWrite(PIN_RS485ENA, LOW);
  if (pkt[0] == (ADDR_ALL | 128)) 
  {
    // ignore all bus data for max reply time
    unsigned long ts0 = millis();
    while (timeDiff(ts0, millis()) < REPLY_WAIT_MSEC)
      Serial1.read();
    return 0;
  }
  size_t rCnt = 0;
  unsigned long ts0 = millis();
  while ((timeDiff(ts0, millis()) < REPLY_WAIT_MSEC) && (rCnt < (len + resLen)))
  {
    int d = Serial1.read();
    if (d < 0)
      continue;
    rxBuf[rCnt++] = d;
  }
  dumpPkt("# RX", rxBuf, rCnt);
  if ((rCnt < len) || (memcmp(pkt, rxBuf, len) != 0))
    return ERR_TX;
  if (rCnt == len)
    return ERR_TO;
  if (rCnt < (len + resLen))
    return ERR_LEN;
  if (resLen < 2)
    return ERR_ERR;
  uint8_t chk = 0xff;
  for (size_t t = 0; t < (resLen - 1); t++)
    chk += rxBuf[len + t];
  chk &= 127;
  if (rxBuf[len + resLen - 1] != chk)
    return ERR_CHK;
  memcpy(res, rxBuf + len, resLen);
  return resLen;
}

void setup()
{
  pinMode(PIN_LED, OUTPUT);
  digitalWrite(PIN_LED, LOW);
  pinMode(PIN_RS485ENA, OUTPUT);
  digitalWrite(PIN_RS485ENA, LOW);
  Serial.begin(115200);
  Serial1.begin(115200);
  delay(1000);
  Serial.println("");
  Serial.println("");
  Serial.println("# RS485 bus interface v0.1 by KVP in 2024");
  uint8_t cmd[3];
  cmd[0] = ADDR_ALL;
  cmd[1] = CMD_STOP_ALL;
  uint8_t res[2];
  rs485txrx(cmd, 3, res, 2);
  Serial.println("READY");
}

bool strStartsWith(const char* s, const char* d, int l)
{
  return(strncmp(s, d, l) == 0);
}

void printHex(uint8_t v)
{
  Serial.print(" ");
  Serial.print((v >> 4) & 15, HEX);
  Serial.print(v & 15, HEX);
}

void sendError(const char* errs, const char* cmd)
{
  Serial.print(errs);
  Serial.print(" ");
  Serial.println(cmd);
}

void sendResult(int res)
{
  switch (res)
  {
    case ERR_ERR:
      Serial.println("ERROR");
      break;
    case ERR_LEN:
      Serial.println("ELEN");
      break;
    case ERR_TO:
      Serial.println("ETO");
      break;
    case ERR_TX:
      Serial.println("ETX");
      break;
    case ERR_CHK:
      Serial.println("ECHK");
      break;
    default:
      Serial.print("OK");
      printHex(res);
      Serial.println("");
      break;
  }
}

static const char* ErrorOk = "OK";
static const char* ErrorSyntax = "ESYN";
static const char* ErrorValue = "EVAL";

void commandIn(const char* cmd)
{
  uint8_t pkt[32];
  uint8_t res[64];
  int rLen;
  if ((strcmp(cmd, "help") == 0) || (strcmp(cmd, "H") == 0))
  {
    Serial.println("# Commands:");
    Serial.println("# help : H : this help text");
    Serial.println("# signal set : SS <address7 (hex)> <uint32_t data (hex)> : set signal aspects (blink0_16:blink1_16)");
    Serial.println("# motor.set : MS  <address7 (hex)> <speed (-63...63, +/-0..100%)> x4 : set motor output speed");
    Serial.println("# motor.pulse : MP <address7 (hex)> <direction [+/-]><pulse (1..8190 msec, 0 = stop, 8191 = continous)> x4 : pulse motor output");
    Serial.println("# sensor get : SG <address7 (hex)> : get sensor data -> 'OK <val0> <min0> <max0> <val1> <min1> <max1> <val2> <min2> <max2> <val3> <min3> <max3>");
    Serial.println("# stop all : SA");
    Serial.println("# pnp list : PL -> 'info <addr> <type> <status>' *N, 'OK'");
    Serial.println("# pnp get : PG <address7 (hex)> -> 'OK <type> <status>'");
    Serial.println("# errors: OK, Esyn (syntax), Eval (value), Elen (length), Eto (timeout), Etx (transmit), Echk (checksum)");
    Serial.println(ErrorOk);
    return;
  } else
  if (strcmp(cmd, "SA") == 0)
  {
    pkt[0] = ADDR_ALL;
    pkt[1] = CMD_STOP_ALL;
    rs485txrx(pkt, 3, res, 2);
    Serial.println(ErrorOk);
  } else
  if (strStartsWith(cmd, "SS ", 3))
  {
    unsigned addr = 0;
    unsigned long data = 0;
    if (sscanf(cmd + 3, "%02x %08lx", &addr, &data) != 2)
    {
      sendError(ErrorSyntax, cmd);
      return;
    }
    pkt[0] = addr;
    pkt[1] = CMD_SIGNAL_SET;
    pkt[2] = data & 127;
    pkt[3] = (data >> 7) & 127;
    pkt[4] = (data >> 14) & 127;
    pkt[5] = (data >> 21) & 127;
    pkt[6] = (data >> 28) & 15;
    rLen = rs485txrx(pkt, 8, res, 2);
    if (rLen < 0)
      sendResult(rLen);
    else
      sendResult(res[0]);
    return;
  } else
  if (strStartsWith(cmd, "MS ", 3))
  {
    unsigned addr = 0;
    int m[4];
    if (sscanf(cmd + 3, "%02x %d %d %d %d", &addr, m + 0, m + 1, m + 2, m + 3) != 5)
    {
      sendError(ErrorSyntax, cmd);
      return;
    }
    for (int t = 0; t < 4; t++)
    {
      if ((m[t] > 63) || (m[t] < -63))
      {
        sendError(ErrorValue, cmd);
        return;
      }
    }
    pkt[0] = addr;
    pkt[1] = CMD_MOTOR_SET;
    for (int t = 0; t < 4; t++)
    {
      if (m[t] < 0)
        pkt[2 + t] = (-m[t]) | 64;
      else
        pkt[2 + t] = m[t];
      /*
      int v;
      if ((pkt[2 + t] & 64) != 0)
        v = -((pkt[2 + t] & 63) << 2);
      else
        v = (pkt[2 + t] & 63) << 2;
      */
    }
    rLen = rs485txrx(pkt, 7, res, 2);
    if (rLen < 0)
      sendResult(rLen);
    else
      sendResult(res[0]);
  } else
  if (strStartsWith(cmd, "MP ", 3))
  {
    unsigned addr = 0;
    int m[4];
    if (sscanf(cmd + 3, "%02x %d %d %d %d", &addr, m + 0, m + 1, m + 2, m + 3) != 5)
    {
      sendError(ErrorSyntax, cmd);
      return;
    }
    for (int t = 0; t < 4; t++)
    {
      if ((m[t] > 8191) || (m[t] < -8191))
      {
        sendError(ErrorValue, cmd);
        return;
      }
    }
    pkt[0] = addr;
    pkt[1] = CMD_MOTOR_PULSE;
    for (int t = 0; t < 4; t++)
    {
      if (m[t] < 0)
      {
        unsigned d = -m[t];
        pkt[2 + t * 2] = d & 127;
        pkt[2 + t * 2 + 1] = ((d >> 7) & 63) | 64;
      } else
      {
        unsigned d = m[t];
        pkt[2 + t * 2] = d & 127;
        pkt[2 + t * 2 + 1] = (d >> 7) & 63;
      }
    }
    rLen = rs485txrx(pkt, 11, res, 2);
    if (rLen < 0)
      sendResult(rLen);
    else
      sendResult(res[0]);
  } else
  if (strStartsWith(cmd, "SG ", 3))
  {
    unsigned addr = 0;
    if (sscanf(cmd + 3, "%02x", &addr) != 1)
    {
      sendError(ErrorSyntax, cmd);
      return;
    }
    pkt[0] = addr;
    pkt[1] = CMD_SENSOR_GET;
    rLen = rs485txrx(pkt, 3, res, 14);
    if (rLen < 0)
    {
      Serial.println(rLen);
      sendResult(rLen);
      return;
    }
    Serial.print("OK");
    for (size_t t = 0; t < (1+4*3); t++)
    {
      Serial.print(" ");
      Serial.print(res[t], HEX);
    }
    Serial.println();
    return;
  } else
  if (strcmp(cmd, "PL") == 0)
  {
    for (int addr = 1; addr < 127; addr++)
    {
      pkt[0] = addr;
      pkt[1] = CMD_INFO;
      rLen = rs485txrx(pkt, 3, res, 3);
      Serial.print("info");
      printHex(addr);
      Serial.print(" ");
      if (rLen < 0)
      {
        sendResult(rLen);
      } else
      {
        Serial.print("OK");
        printHex(res[0]);
        printHex(res[1]);
        Serial.println("");
      }
    }
    Serial.println(ErrorOk);
  } else
  if (strStartsWith(cmd, "PG ", 3))
  {
    unsigned addr = 0;
    if (sscanf(cmd + 3, "%02x", &addr) != 1)
    {
      sendError(ErrorSyntax, cmd);
      return;
    }
    pkt[0] = addr;
    pkt[1] = CMD_INFO;
    rLen = rs485txrx(pkt, 3, res, 3);
    if (rLen < 0)
      sendResult(rLen);
    Serial.print(ErrorOk);
    printHex(res[0]);
    printHex(res[1]);
    Serial.println("");
  }
}

static char cmdBuf[256];
static size_t cmdBufLen = 0;

void loop()
{
  int ch = Serial.read();
  if (ch > 0)
  {
    switch (ch)
    {
      case '\r':
        break;
      case '\n':
        cmdBuf[cmdBufLen] = 0;
        if ((cmdBufLen > 0) && (cmdBuf[0] != '#'))
          commandIn(cmdBuf);
        cmdBufLen = 0;
        break;
      default:
        if (cmdBufLen < (sizeof(cmdBuf) - 1))
          cmdBuf[cmdBufLen++] = ch;
        break;
    }
  }
}
