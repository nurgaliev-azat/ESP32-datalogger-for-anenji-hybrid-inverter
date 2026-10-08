#include <Arduino.h>
#include <WiFi.h>
#include <WiFiUdp.h>
#include <EEPROM.h>
#include <Preferences.h>
#include <time.h>
#include <vector>

#include "Registers.h"
#include "web_server.h"

constexpr uint16_t UDP_PORT=58899;
constexpr uint16_t TCP_PORT=8899;
constexpr uint8_t MODBUS_UNIT_ID=1;
constexpr uint8_t MODBUS_FUNCTION=3;
constexpr uint32_t TCP_CONNECT_TIMEOUT_MS=30000;
constexpr uint32_t MODBUS_RESPONSE_TIMEOUT_MS=5000;
constexpr size_t MAX_RESPONSE_SIZE=80;

struct ReadRange { uint16_t start,count; };
const ReadRange READ_RANGES[]={{100,2},{108,2},{201,34}};
constexpr size_t READ_RANGE_COUNT=
  sizeof(READ_RANGES)/sizeof(READ_RANGES[0]);

constexpr size_t EEPROM_SIZE=8192;
constexpr uint32_t EEPROM_MAGIC=0x414E454A;
constexpr uint16_t EEPROM_VERSION=1;

struct HistoryRecord {
  uint16_t year;
  uint8_t month,day,hour;
  uint32_t gridWs,pvWs,loadWs,batteryChargeWs,batteryDischargeWs;
};

struct HistoryHeader {
  uint32_t magic;
  uint16_t version,count,writeIndex,reserved;
};

constexpr size_t MAX_HISTORY_RECORDS=
  (EEPROM_SIZE-sizeof(HistoryHeader))/sizeof(HistoryRecord);

struct InstantPower {
  float grid,pv,load,batteryCharge,batteryDischarge;
};

struct EnergyAccumulator {
  float gridWs,pvWs,loadWs,batteryChargeWs,batteryDischargeWs;
};

WiFiUDP udp;
WiFiServer tcpServer(TCP_PORT);
WiFiClient inverterClient;
Preferences preferences;

WebSettings appSettings;
InstantPower previousPower={},currentPower={};
EnergyAccumulator currentHourEnergy={};
bool previousPowerValid=false;
uint32_t previousMeasurementSeconds=0;
uint32_t lastFiveMinuteSampleSeconds=0;
uint32_t currentDayNumber=0;
int64_t activeHourKey=-1;

float currentGridVoltage=0;
float currentPvVoltage=0;
float currentLoadVoltage=0;
float currentBatteryVoltage=0;
uint8_t currentBatteryPercent=0;

String currentStatus="Waiting for data";
String currentUpdateTime="-";
String currentMode="Ожидание данных";
String currentFaults="Неполадок нет";
String currentWarnings="Предупреждений нет";
bool faultRead=false,warningRead=false,modeRead=false;

std::vector<WebSample> todaySamples;
bool setupAccessPoint=false;

void fillWebLiveData(WebLiveData& data);
size_t getTodaySamples(WebSample* output,size_t maxSamples);
size_t getHistoryForWeb(WebHistoryHour* output,size_t maxRecords,
                        const String& scale);
void saveSettings(const WebSettings& settings);

uint16_t crc16Modbus(const uint8_t* data,size_t length){
  uint16_t crc=0xFFFF;
  for(size_t i=0;i<length;i++){
    crc^=data[i];
    for(uint8_t b=0;b<8;b++)
      crc=(crc&1)?(crc>>1)^0xA001:crc>>1;
  }
  return crc;
}

int32_t signedRegister(uint16_t raw){
  return raw>=32768?static_cast<int32_t>(raw)-65536:raw;
}

void printHexByte(uint8_t value){
  if(value<16)Serial.print('0');
  Serial.print(value,HEX);
}

void printHexBuffer(const uint8_t* data,size_t length){
  for(size_t i=0;i<length;i++){
    printHexByte(data[i]);
    if(i+1<length)Serial.print(' ');
  }
  Serial.println();
}

const RegisterDefinition* findRegisterDefinition(uint16_t address){
  for(size_t i=0;i<ANENJI_REGISTER_COUNT;i++)
    if(ANENJI_REGISTERS[i].address==address)
      return &ANENJI_REGISTERS[i];
  return nullptr;
}

bool getRegisterValue(uint16_t start,uint16_t count,const uint8_t* bytes,
                      uint16_t address,uint16_t& value){
  if(address<start || address>=start+count)return false;
  uint16_t i=address-start;
  value=(static_cast<uint16_t>(bytes[i*2])<<8)|bytes[i*2+1];
  return true;
}

bool validClock(){
  return time(nullptr)>=100000;
}

String currentDateTimeString(){
  time_t now=time(nullptr);
  if(now<100000)return "-";
  struct tm t;
  localtime_r(&now,&t);
  char text[32];
  snprintf(text,sizeof(text),"%04d-%02d-%02d %02d:%02d:%02d",
    t.tm_year+1900,t.tm_mon+1,t.tm_mday,t.tm_hour,t.tm_min,t.tm_sec);
  return String(text);
}

uint32_t currentDayId(){
  time_t now=time(nullptr);
  if(now<100000)return 0;
  struct tm t;
  localtime_r(&now,&t);
  return static_cast<uint32_t>(t.tm_year)*400UL+
    static_cast<uint32_t>(t.tm_yday)+1;
}

void synchronizeTime(){
  configTime(static_cast<long>(appSettings.timezone)*3600L,0,
             "pool.ntp.org","time.nist.gov");
  Serial.println("Синхронизация NTP...");
  uint32_t started=millis();
  while(!validClock() && millis()-started<15000)delay(500);
  Serial.println(validClock()?"Время синхронизировано":
                              "Время не синхронизировано");
}

void loadSettings(){
  preferences.begin("anenji",true);
  appSettings.ssid=preferences.getString("ssid","WI-FI_SSID");
  appSettings.password=preferences.getString("password","Password");
  appSettings.inverterIp=preferences.getString("inverterIp","192.168.0.148");
  appSettings.pageUpdateSeconds=preferences.getUInt("pageUpdate",5);
  appSettings.registerIntervalSeconds=
    preferences.getUInt("registerInterval",30);
  appSettings.timezone=preferences.getChar("timezone",5);
  appSettings.language=preferences.getString("language","en");
  preferences.end();

  if(appSettings.pageUpdateSeconds<1 ||
     appSettings.pageUpdateSeconds>3600)
    appSettings.pageUpdateSeconds=5;
  if(appSettings.registerIntervalSeconds<5 ||
     appSettings.registerIntervalSeconds>3600)
    appSettings.registerIntervalSeconds=30;
  if(appSettings.timezone< -12 || appSettings.timezone>14)
    appSettings.timezone=5;
  if(appSettings.language!="ru" && appSettings.language!="en")
    appSettings.language="en";
}

void saveSettings(const WebSettings& settings){
  appSettings=settings;
  preferences.begin("anenji",false);
  preferences.putString("ssid",appSettings.ssid);
  preferences.putString("password",appSettings.password);
  preferences.putString("inverterIp",appSettings.inverterIp);
  preferences.putUInt("pageUpdate",appSettings.pageUpdateSeconds);
  preferences.putUInt("registerInterval",appSettings.registerIntervalSeconds);
  preferences.putChar("timezone",appSettings.timezone);
  preferences.putString("language",appSettings.language);
  preferences.end();
  Serial.println("Настройки сохранены");
}

bool connectToWiFi(){
  WiFi.mode(WIFI_STA);
  WiFi.begin(appSettings.ssid.c_str(),appSettings.password.c_str());
  Serial.print("Подключение к Wi-Fi");
  uint32_t started=millis();
  while(WiFi.status()!=WL_CONNECTED && millis()-started<20000){
    Serial.print('.');
    delay(500);
  }
  Serial.println();
  if(WiFi.status()!=WL_CONNECTED)return false;
  Serial.print("IP ESP32: ");
  Serial.println(WiFi.localIP());
  return true;
}

void startSetupAccessPoint(){
  if(setupAccessPoint)return;
  setupAccessPoint=true;
  WiFi.mode(WIFI_AP);
  WiFi.softAP("Anenji-setup","12345678");
  Serial.print("Точка доступа, IP: ");
  Serial.println(WiFi.softAPIP());
  beginWebServer(fillWebLiveData,getTodaySamples,getHistoryForWeb,
                 saveSettings,appSettings,true);
}

bool notifyDatalogger(){
  IPAddress ip;
  if(!ip.fromString(appSettings.inverterIp)){
    Serial.println("Некорректный IP инвертора");
    return false;
  }
  String command="set>server="+WiFi.localIP().toString()+
                 ":"+String(TCP_PORT)+";";
  if(!udp.begin(0))return false;
  udp.beginPacket(ip,UDP_PORT);
  udp.print(command);
  udp.endPacket();
  Serial.print("UDP: ");
  Serial.println(command);

  uint32_t started=millis();
  while(millis()-started<2000){
    if(udp.parsePacket()>0){
      Serial.print("UDP-ответ: ");
      while(udp.available())Serial.write(static_cast<uint8_t>(udp.read()));
      Serial.println();
      break;
    }
    delay(10);
  }
  udp.stop();
  return true;
}

bool ensureInverterConnection(){
  if(inverterClient && inverterClient.connected())return true;
  inverterClient.stop();
  if(!notifyDatalogger())return false;
  uint32_t started=millis();
  while(millis()-started<TCP_CONNECT_TIMEOUT_MS){
    WiFiClient client=tcpServer.available();
    if(client){
      inverterClient=client;
      Serial.print("Инвертор подключён: ");
      Serial.println(inverterClient.remoteIP());
      return true;
    }
    handleWebServer();
    delay(10);
  }
  Serial.println("TCP-подключение не получено");
  return false;
}

void buildReadRequest(uint16_t start,uint16_t count,uint8_t request[8]){
  request[0]=MODBUS_UNIT_ID;
  request[1]=MODBUS_FUNCTION;
  request[2]=highByte(start);
  request[3]=lowByte(start);
  request[4]=highByte(count);
  request[5]=lowByte(count);
  uint16_t crc=crc16Modbus(request,6);
  request[6]=lowByte(crc);
  request[7]=highByte(crc);
}

bool readExactly(WiFiClient& client,uint8_t* dest,size_t count,
                 uint32_t timeoutMs){
  size_t received=0;
  uint32_t started=millis();
  while(received<count){
    while(client.available() && received<count){
      int v=client.read();
      if(v>=0)dest[received++]=static_cast<uint8_t>(v);
    }
    if(received==count)return true;
    if(millis()-started>=timeoutMs)return false;
    if(!client.connected() && !client.available())return false;
    handleWebServer();
    delay(1);
  }
  return true;
}

size_t historyRecordAddress(size_t index){
  return sizeof(HistoryHeader)+index*sizeof(HistoryRecord);
}

void initializeHistoryHeader(HistoryHeader& header){
  EEPROM.get(0,header);
  if(header.magic!=EEPROM_MAGIC ||
     header.version!=EEPROM_VERSION ||
     header.count>MAX_HISTORY_RECORDS ||
     header.writeIndex>=MAX_HISTORY_RECORDS){
    header={};
    header.magic=EEPROM_MAGIC;
    header.version=EEPROM_VERSION;
    EEPROM.put(0,header);
    EEPROM.commit();
  }
}

uint32_t storedWs(float value){
  if(value<=0)return 0;
  if(value>=4294967000.0f)return UINT32_MAX;
  return static_cast<uint32_t>(value);
}

void saveHourToEeprom(const EnergyAccumulator& energy,time_t hourStart){
  if(hourStart<100000)return;
  HistoryHeader header;
  initializeHistoryHeader(header);
  struct tm t;
  localtime_r(&hourStart,&t);

  HistoryRecord r={};
  r.year=t.tm_year+1900;
  r.month=t.tm_mon+1;
  r.day=t.tm_mday;
  r.hour=t.tm_hour;
  r.gridWs=storedWs(energy.gridWs);
  r.pvWs=storedWs(energy.pvWs);
  r.loadWs=storedWs(energy.loadWs);
  r.batteryChargeWs=storedWs(energy.batteryChargeWs);
  r.batteryDischargeWs=storedWs(energy.batteryDischargeWs);

  EEPROM.put(historyRecordAddress(header.writeIndex),r);
  header.writeIndex=(header.writeIndex+1)%MAX_HISTORY_RECORDS;
  if(header.count<MAX_HISTORY_RECORDS)header.count++;
  EEPROM.put(0,header);
  EEPROM.commit();
  Serial.printf("Сохранён час %04u-%02u-%02u %02u:00\n",
                r.year,r.month,r.day,r.hour);
}

size_t readAllHistory(HistoryRecord* output,size_t maxRecords){
  HistoryHeader header;
  initializeHistoryHeader(header);
  size_t count=min(static_cast<size_t>(header.count),maxRecords);
  size_t first=header.count<MAX_HISTORY_RECORDS?0:header.writeIndex;
  for(size_t i=0;i<count;i++)
    EEPROM.get(historyRecordAddress((first+i)%MAX_HISTORY_RECORDS),
               output[i]);
  return count;
}

float positivePower(float x){return x>0?x:0;}

InstantPower makeInstantPower(float grid,float pv,float load,float battery){
  InstantPower p={};
  p.grid=positivePower(grid);
  p.pv=positivePower(pv);
  p.load=positivePower(load);
  if(battery>=0)p.batteryCharge=battery;
  else p.batteryDischarge=-battery;
  return p;
}

void integrateEnergy(const InstantPower& a,const InstantPower& b,
                     uint32_t seconds){
  if(!seconds || seconds>3600)return;
  currentHourEnergy.gridWs+=(a.grid+b.grid)*.5f*seconds;
  currentHourEnergy.pvWs+=(a.pv+b.pv)*.5f*seconds;
  currentHourEnergy.loadWs+=(a.load+b.load)*.5f*seconds;
  currentHourEnergy.batteryChargeWs+=
    (a.batteryCharge+b.batteryCharge)*.5f*seconds;
  currentHourEnergy.batteryDischargeWs+=
    (a.batteryDischarge+b.batteryDischarge)*.5f*seconds;
}

void processHourChange(){
  time_t now=time(nullptr);
  if(now<100000)return;
  int64_t key=static_cast<int64_t>(now)/3600;
  if(activeHourKey<0){
    activeHourKey=key;
    return;
  }
  if(key==activeHourKey)return;
  // Ключ — граница часа по Unix-времени; с заданным целочасовым UTC
  // смещением она совпадает с границей местного часа.
  if(key==activeHourKey+1)
    saveHourToEeprom(currentHourEnergy,
                     static_cast<time_t>(activeHourKey*3600));
  else
    Serial.println("Пропуск записи: часы изменились больше чем на час");
  currentHourEnergy={};
  activeHourKey=key;
  previousPowerValid=false; // не переносить интервал через границу часа
}

void resetSamplesAtNewDay(){
  uint32_t day=currentDayId();
  if(!day)return;
  if(currentDayNumber && day!=currentDayNumber)todaySamples.clear();
  currentDayNumber=day;
}

void saveFiveMinuteSample(){
  if(!previousPowerValid || !validClock())return;
  uint32_t nowSeconds=millis()/1000UL;
  if(lastFiveMinuteSampleSeconds &&
     nowSeconds-lastFiveMinuteSampleSeconds<300)return;

  resetSamplesAtNewDay();
  time_t now=time(nullptr);
  struct tm t;
  localtime_r(&now,&t);

  WebSample s={};
  s.timestamp=static_cast<uint32_t>(now);
  s.minuteOfDay=t.tm_hour*60+t.tm_min;
  s.gridPower=currentPower.grid;
  s.pvPower=currentPower.pv;
  s.loadPower=currentPower.load;
  s.batteryChargePower=currentPower.batteryCharge;
  s.batteryDischargePower=currentPower.batteryDischarge;

  if(todaySamples.size()>=288)todaySamples.erase(todaySamples.begin());
  todaySamples.push_back(s);
  lastFiveMinuteSampleSeconds=nowSeconds;
}

size_t getTodaySamples(WebSample* output,size_t maxSamples){
  resetSamplesAtNewDay();
  size_t count=min(todaySamples.size(),maxSamples);
  for(size_t i=0;i<count;i++)output[i]=todaySamples[i];
  return count;
}

void setWebHour(WebHistoryHour& out,const HistoryRecord& r){
  out={};
  out.year=r.year;out.month=r.month;out.day=r.day;out.hour=r.hour;
  out.gridKwh=r.gridWs/3600000.0f;
  out.pvKwh=r.pvWs/3600000.0f;
  out.loadKwh=r.loadWs/3600000.0f;
  out.batteryChargeKwh=r.batteryChargeWs/3600000.0f;
  out.batteryDischargeKwh=r.batteryDischargeWs/3600000.0f;
}

size_t getHistoryForWeb(WebHistoryHour* output,size_t maxRecords,
                        const String& scale){
  (void)scale;
  static HistoryRecord records[MAX_HISTORY_RECORDS];
  if(!maxRecords)return 0;
  size_t count=readAllHistory(records,MAX_HISTORY_RECORDS);
  // Оставить место для незавершённого текущего часа.
  size_t capacity=maxRecords-(validClock()?1:0);
  size_t first=count>capacity?count-capacity:0;
  size_t result=0;
  for(size_t i=first;i<count;i++)
    setWebHour(output[result++],records[i]);

  if(validClock() && result<maxRecords){
    time_t now=time(nullptr);
    struct tm t;
    localtime_r(&now,&t);
    WebHistoryHour& x=output[result++];
    x={};
    x.year=t.tm_year+1900;
    x.month=t.tm_mon+1;
    x.day=t.tm_mday;
    x.hour=t.tm_hour;
    x.gridKwh=currentHourEnergy.gridWs/3600000.0f;
    x.pvKwh=currentHourEnergy.pvWs/3600000.0f;
    x.loadKwh=currentHourEnergy.loadWs/3600000.0f;
    x.batteryChargeKwh=currentHourEnergy.batteryChargeWs/3600000.0f;
    x.batteryDischargeKwh=currentHourEnergy.batteryDischargeWs/3600000.0f;
  }
  return result;
}

String diagnosticText(uint32_t flags,bool faults){
  if(!flags)return faults?"Неполадок нет":"Предупреждений нет";
  String text=faults?"Неполадки: ":"Предупреждения: ";
  bool first=true;
  for(uint8_t bit=0;bit<32;bit++){
    if(!(flags&(UINT32_C(1)<<bit)))continue;
    if(!first)text+=F("; ");
    first=false;
    text+=faults?faultName(bit):warningName(bit);
    text+=F(" [");
    text+=String(bit);
    text+=']';
    if(text.length()>210){
      text+=F("; …");
      break;
    }
  }
  return text;
}

void processDiagnosticRegisters(uint16_t start,uint16_t count,
                                const uint8_t* bytes){
  if(count<2 || (start!=100 && start!=108))return;
  uint32_t flags=(static_cast<uint32_t>(bytes[0])<<24)|
    (static_cast<uint32_t>(bytes[1])<<16)|
    (static_cast<uint32_t>(bytes[2])<<8)|bytes[3];
  if(start==100){
    currentFaults=diagnosticText(flags,true);
    faultRead=true;
  }else{
    currentWarnings=diagnosticText(flags,false);
    warningRead=true;
  }
}

void processPowerRegisters(uint16_t start,uint16_t count,
                           const uint8_t* bytes){
  if(start!=201)return;
  uint16_t grid=0,pv=0,load=0,battery=0;
  uint16_t gv=0,pvV=0,lv=0,bv=0,mode=0,percent=0;
  if(!getRegisterValue(start,count,bytes,204,grid) ||
     !getRegisterValue(start,count,bytes,223,pv) ||
     !getRegisterValue(start,count,bytes,213,load) ||
     !getRegisterValue(start,count,bytes,217,battery))return;

  getRegisterValue(start,count,bytes,202,gv);
  getRegisterValue(start,count,bytes,219,pvV);
  getRegisterValue(start,count,bytes,210,lv);
  getRegisterValue(start,count,bytes,215,bv);
  getRegisterValue(start,count,bytes,201,mode);
  getRegisterValue(start,count,bytes,229,percent);

  // Сначала закрыть завершённый час, затем учитывать новые показания.
  processHourChange();
  currentPower=makeInstantPower(
    signedRegister(grid),signedRegister(pv),
    signedRegister(load),signedRegister(battery));
  currentGridVoltage=signedRegister(gv)*.1f;
  currentPvVoltage=signedRegister(pvV)*.1f;
  currentLoadVoltage=signedRegister(lv)*.1f;
  currentBatteryVoltage=signedRegister(bv)*.1f;
  currentBatteryPercent=static_cast<uint8_t>(min(
    static_cast<uint16_t>(100),percent));
  currentMode=workingModeName(mode);
  modeRead=true;

  uint32_t nowSeconds=millis()/1000UL;
  if(previousPowerValid){
    integrateEnergy(previousPower,currentPower,
                    nowSeconds-previousMeasurementSeconds);
  }
  previousPower=currentPower;
  previousPowerValid=true;
  previousMeasurementSeconds=nowSeconds;
  currentStatus="Data updated";
  currentUpdateTime=currentDateTimeString();
  saveFiveMinuteSample();
}

void printRegisterValue(uint16_t address,uint16_t raw){
  const RegisterDefinition* def=findRegisterDefinition(address);
  Serial.print("Register ");
  Serial.print(address);
  Serial.print(" | HEX 0x");
  printHexByte(highByte(raw));
  printHexByte(lowByte(raw));
  Serial.print(" | ");
  if(!def){
    Serial.println(raw);
    return;
  }
  Serial.print(def->title);
  Serial.print(" = ");
  float value=def->type==REG_SHORT?signedRegister(raw)*def->scale:
                                    raw*def->scale;
  Serial.print(value,2);
  if(def->unit && def->unit[0]){
    Serial.print(' ');
    Serial.print(def->unit);
  }
  if(address==201){
    Serial.print(" (");
    Serial.print(workingModeName(raw));
    Serial.print(')');
  }
  Serial.println();
}

void printRegisterRange(uint16_t start,uint16_t count,
                        const uint8_t* bytes){
  Serial.println("----------------------------------------");
  Serial.printf("Диапазон %u-%u\n",start,start+count-1);
  for(uint16_t i=0;i<count;i++){
    uint16_t raw=(static_cast<uint16_t>(bytes[i*2])<<8)|
                 bytes[i*2+1];
    printRegisterValue(start+i,raw);
  }
  processDiagnosticRegisters(start,count,bytes);
  processPowerRegisters(start,count,bytes);
}

bool readModbusRange(uint16_t start,uint16_t count){
  if(!count || count>34 || !ensureInverterConnection())return false;
  uint8_t request[8];
  buildReadRequest(start,count,request);
  Serial.print("Modbus request: ");
  printHexBuffer(request,sizeof(request));

  if(inverterClient.write(request,sizeof(request))!=sizeof(request)){
    inverterClient.stop();
    return false;
  }
  uint8_t response[MAX_RESPONSE_SIZE]={};
  if(!readExactly(inverterClient,response,3,MODBUS_RESPONSE_TIMEOUT_MS)){
    Serial.println("Тайм-аут заголовка");
    inverterClient.stop();
    return false;
  }
  size_t length=(response[1]&0x80)?5:3+response[2]+2;
  if(length>sizeof(response) || length<5 ||
     !readExactly(inverterClient,response+3,length-3,
                  MODBUS_RESPONSE_TIMEOUT_MS)){
    Serial.println("Неверная длина или тайм-аут ответа");
    inverterClient.stop();
    return false;
  }
  Serial.print("Modbus response: ");
  printHexBuffer(response,length);
  uint16_t crc=crc16Modbus(response,length-2);
  uint16_t received=response[length-2]|
                    (static_cast<uint16_t>(response[length-1])<<8);
  if(crc!=received || response[0]!=MODBUS_UNIT_ID){
    Serial.println("Ошибка CRC или Unit ID");
    inverterClient.stop();
    return false;
  }
  if(response[1]==(MODBUS_FUNCTION|0x80)){
    Serial.printf("Исключение Modbus: %u\n",response[2]);
    return false;
  }
  if(response[1]!=MODBUS_FUNCTION || response[2]!=count*2){
    Serial.println("Неверный ответ Modbus");
    return false;
  }
  printRegisterRange(start,count,response+3);
  return true;
}

void copyWebText(char* destination,size_t capacity,const String& source){
  if(!capacity)return;
  strlcpy(destination,source.c_str(),capacity);
}

void fillWebLiveData(WebLiveData& d){
  d={};
  d.valid=previousPowerValid;
  copyWebText(d.status,sizeof(d.status),currentStatus);
  copyWebText(d.updateTime,sizeof(d.updateTime),currentUpdateTime);
  copyWebText(d.inverterIp,sizeof(d.inverterIp),appSettings.inverterIp);
  copyWebText(d.mode,sizeof(d.mode),
              modeRead?currentMode:String("Ожидание данных"));
  copyWebText(d.faults,sizeof(d.faults),
              faultRead?currentFaults:String("Диагностика неполадок недоступна"));
  copyWebText(d.warnings,sizeof(d.warnings),
              warningRead?currentWarnings:
                          String("Диагностика предупреждений недоступна"));
  d.gridPower=currentPower.grid;
  d.pvPower=currentPower.pv;
  d.loadPower=currentPower.load;
  d.batteryChargePower=currentPower.batteryCharge;
  d.batteryDischargePower=currentPower.batteryDischarge;
  d.batteryPower=currentPower.batteryCharge-
                 currentPower.batteryDischarge;
  d.gridVoltage=currentGridVoltage;
  d.pvVoltage=currentPvVoltage;
  d.loadVoltage=currentLoadVoltage;
  d.batteryVoltage=currentBatteryVoltage;
  d.batteryPercent=currentBatteryPercent;
  d.gridKwh=currentHourEnergy.gridWs/3600000.0f;
  d.pvKwh=currentHourEnergy.pvWs/3600000.0f;
  d.loadKwh=currentHourEnergy.loadWs/3600000.0f;
  d.batteryChargeKwh=currentHourEnergy.batteryChargeWs/3600000.0f;
  d.batteryDischargeKwh=
    currentHourEnergy.batteryDischargeWs/3600000.0f;
}

void setup(){
  Serial.begin(115200);
  delay(1000);
  Serial.println("\nANENJI Datalogger / ESP32");
  loadSettings();
  EEPROM.begin(EEPROM_SIZE);
  HistoryHeader header;
  initializeHistoryHeader(header);

  if(!connectToWiFi()){
    startSetupAccessPoint();
    return;
  }
  synchronizeTime();
  processHourChange();
  tcpServer.begin();
  beginWebServer(fillWebLiveData,getTodaySamples,getHistoryForWeb,
                 saveSettings,appSettings,false);
  Serial.printf("Веб-сервер запущен; часовых записей: %u\n",
                static_cast<unsigned>(MAX_HISTORY_RECORDS));
}

void loop(){
  handleWebServer();
  if(setupAccessPoint){
    delay(5);
    return;
  }

  if(WiFi.status()!=WL_CONNECTED){
    inverterClient.stop();
    if(!connectToWiFi()){
      startSetupAccessPoint();
      return;
    }
    synchronizeTime();
    tcpServer.begin();
  }

  static uint32_t lastReadMillis=0;
  uint32_t nowMillis=millis();
  if(nowMillis-lastReadMillis>=
     appSettings.registerIntervalSeconds*1000UL){
    lastReadMillis=nowMillis;
    for(size_t i=0;i<READ_RANGE_COUNT;i++){
      readModbusRange(READ_RANGES[i].start,READ_RANGES[i].count);
      handleWebServer();
      delay(200);
    }
  }
  processHourChange();
  saveFiveMinuteSample();
  delay(2);
}
