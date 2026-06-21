// Milli Stem ERATOSTHENES KALBURU Yöntemi Asalmatik
#include <WiFi.h>
#include <WebSocketsServer.h>
#include <TM1637Display.h>
#include <Wire.h>
#include <Deneyap_KumandaKolu.h>

// ---------- DONANIM ----------
#define CLK D0            // D0 (GPIO5)
#define DIO D1            // D1 (GPIO4)
#define DAHILI_BUTON D25  // D25 (GPIO25) – sıfırlama butonu

TM1637Display display(CLK, DIO);
Joystick KumandaKolu;

// ---------- JOYSTICK EŞİKLERİ ----------
#define ESIK_SAG  800
#define ESIK_SOL  200

// ---------- WI-FI ----------
const char* evSSID = "EmreEmir Modem 2.4G";
const char* evSifre = "eE1232222";
const char* apSSID = "Deneyap_AsalMatik";
const char* apSifre = "matematik123";
WiFiServer server(80);
WebSocketsServer webSocket = WebSocketsServer(81);

// ---------- OYUN DURUMU ----------
uint8_t sayiDurumlari[101];
uint8_t sayiBolenMask[101];
int asalIndeks[101];
int barSecili = 0;
bool barSecildi[4] = {false};
bool joystickVar = false;
bool oyunBitti = false;
unsigned long oyunBitisZamani = 0;

unsigned long sonHareket = 0, sonButon = 0, sonJoyKontrol = 0, sonDahiliButon = 0;
const unsigned long gecikmeHareket = 200, gecikmeButon = 500, gecikmeJoyKontrol = 1000;
const unsigned long dahiliButonGecikmesi = 300;

// ---------- RENK PALETİ ----------
const char* renkPaleti[10] = {
  "#FF0000", "#00FF00", "#0000FF", "#FFFF00",
  "#FF00FF", "#00FFFF", "#FFA500", "#800080", "#008080", "#FF4500"
};

// ---------- HTML ARAYÜZÜ (gri asallar + tüm asallara animasyon) ----------
const char HTML_INTERFACE[] PROGMEM = R"=====(
<!DOCTYPE html>
<html>
<head>
<meta charset="utf-8">
<meta name="viewport" content="width=device-width, initial-scale=1.0">
<title>AsalMatik</title>
<style>
  body { font-family:'Segoe UI',Arial,sans-serif; background:#1A1A1A; color:#fff; text-align:center; margin:0; padding:10px; }
  .container { display:flex; flex-direction:column; align-items:center; }
  .prime-bar { display:flex; gap:12px; margin-bottom:10px; }
  .bar-cell { width:55px; height:55px; border-radius:8px; display:flex; align-items:center; justify-content:center; font-size:22px; font-weight:bold; transition:all 0.1s; }
  .bar-unselected { background:#444; color:#aaa; }
  .bar-selected { background:#888; color:#ccc; }
  .bar-cell.active { border:3px solid #FFCC00; transform:scale(1.1); box-shadow:0 0 12px #FFCC00; }
  .grid-container { display:flex; flex-wrap:wrap; max-width:1000px; gap:20px; justify-content:center; align-items:flex-start; }
  .grid { display:grid; grid-template-columns:repeat(10,1fr); gap:6px; background:#2D2D2D; padding:12px; border-radius:12px; }
  .cell { width:45px; height:45px; background:#3D3D3D; border-radius:6px; display:flex; align-items:center; justify-content:center; font-size:16px; font-weight:bold; transition: transform 0.3s ease; }
  .cell.one { background:#555; color:#888; }
  .cell.prime { color:#fff; }
  .cell.prime-gray { background:#555 !important; color:#aaa !important; }
  .cell.elim { }
  .sidebar { width:280px; background:#2D2D2D; padding:20px; border-radius:12px; text-align:left; }
  h2 { margin-top:0; color:#00D2FF; }
  .stat { margin:12px 0; font-size:15px; }
  .badge { display:inline-block; padding:4px 8px; border-radius:4px; font-weight:bold; font-size:12px; }
  .online { background:#2ECC71; color:#fff; }
  .offline { background:#E74C3C; color:#fff; }
</style>
</head>
<body>
<h1>ERATOSTHENES KALBURU MILLI STEM</h1>
<div class="container">
  <div class="prime-bar" id="primeBar">
    <div class="bar-cell bar-unselected active" id="bar2">2</div>
    <div class="bar-cell bar-unselected" id="bar3">3</div>
    <div class="bar-cell bar-unselected" id="bar5">5</div>
    <div class="bar-cell bar-unselected" id="bar7">7</div>
  </div>
  <div class="grid-container">
    <div class="grid" id="numberGrid"></div>
    <div class="sidebar">
      <h2>KONTROL PANELİ</h2>
      <div class="stat">Bağlantı: <span id="sockStatus" class="badge offline">Bağlanılıyor...</span></div>
      <div class="stat" style="color:#FFCC00;">Seçili Asal: <span id="currPrime">2</span></div>
      <div class="stat">Bulunan Asallar: <span id="primeCount" style="color:#2ECC71;">0</span></div>
      <div class="stat">Elenen Sayılar: <span id="elimCount" style="color:#E74C3C;">0 / 100</span></div>
    </div>
  </div>
</div>
<script>
const renkler = ["#FF0000","#00FF00","#0000FF","#FFFF00","#FF00FF","#00FFFF","#FFA500","#800080","#008080","#FF4500"];
const grid = document.getElementById('numberGrid');
const cells = {};
let sonBarSecili = 0;

for(let i=1; i<=100; i++){
  let d = document.createElement('div');
  d.className = 'cell';
  if(i===1) d.className = 'cell one';
  d.innerText = i;
  grid.appendChild(d);
  cells[i] = d;
}

const asalIndeksMap = {};
const hucreBolenMask = {};

function getBrightness(r, g, b) { return (r*299 + g*587 + b*114) / 1000; }

function renkKaristir(renkDizisi) {
  if(renkDizisi.length === 0) return '#E74C3C';
  let r=0,g=0,b=0;
  renkDizisi.forEach(c => {
    r += parseInt(c.slice(1,3),16);
    g += parseInt(c.slice(3,5),16);
    b += parseInt(c.slice(5,7),16);
  });
  r = Math.min(r, 255); g = Math.min(g, 255); b = Math.min(b, 255);
  return `rgb(${r},${g},${b})`;
}

function hucreyiGuncelle(num) {
  let cell = cells[num];
  if(!cell) return;
  if(cell.classList.contains('prime') || cell.classList.contains('prime-gray')) return;
  if(cell.classList.contains('elim')) {
    let mask = hucreBolenMask[num] || 0;
    let katRenkler = [];
    for(let idx=0; idx<10; idx++) {
      if(mask & (1 << idx)) katRenkler.push(renkler[idx]);
    }
    let bg = renkKaristir(katRenkler);
    cell.style.backgroundColor = bg;
    let rgb = bg.match(/\d+/g);
    let bright = getBrightness(+rgb[0], +rgb[1], +rgb[2]);
    cell.style.color = bright > 128 ? '#000' : '#fff';
  }
}

function barActiveGuncelle(index) {
  ['bar2','bar3','bar5','bar7'].forEach(id => document.getElementById(id).classList.remove('active'));
  if(index >=0) {
    let ids = ['bar2','bar3','bar5','bar7'];
    document.getElementById(ids[index]).classList.add('active');
    sonBarSecili = index;
    document.getElementById('currPrime').innerText = [2,3,5,7][index];
  }
}

function barSecimGuncelle(index, secildi) {
  let ids = ['bar2','bar3','bar5','bar7'];
  let el = document.getElementById(ids[index]);
  if(secildi) {
    el.classList.remove('bar-unselected');
    el.classList.add('bar-selected');
  } else {
    el.classList.remove('bar-selected');
    el.classList.add('bar-unselected');
  }
}

function asallariBuyut() {
  let asalList = [];
  for (let i = 2; i <= 100; i++) {
    if (cells[i] && (cells[i].classList.contains('prime') || cells[i].classList.contains('prime-gray'))) {
      asalList.push(i);
    }
  }
  asalList.forEach((num, index) => {
    setTimeout(() => {
      let cell = cells[num];
      if (cell) {
        cell.style.transition = 'transform 0.3s ease';
        cell.style.transform = 'scale(1.5)';
        setTimeout(() => { cell.style.transform = 'scale(1)'; }, 400);
      }
    }, index * 300);
  });
}

let ws;
function webSocketBaglan() {
  ws = new WebSocket('ws://'+window.location.hostname+':81/');
  ws.onopen = () => {
    document.getElementById('sockStatus').innerText = "BAĞLANDI";
    document.getElementById('sockStatus').className = "badge online";
  };
  ws.onclose = () => {
    document.getElementById('sockStatus').innerText = "KOPMA VAR";
    document.getElementById('sockStatus').className = "badge offline";
    setTimeout(webSocketBaglan, 2000);
  };
  ws.onmessage = (e) => {
    let p = e.data.split(':');
    let cmd = p[0];
    if(cmd === 'BARPOS') barActiveGuncelle(parseInt(p[1]));
    else if(cmd === 'BARSELECT') {
      barSecimGuncelle(parseInt(p[1]), parseInt(p[2]) === 1);
    }
    else if(cmd === 'PRIME') {
      let asal = parseInt(p[1]);
      cells[asal].className = 'cell prime';
      if(p.length > 2) {
        let idx = parseInt(p[2]);
        asalIndeksMap[asal] = idx;
        cells[asal].style.backgroundColor = renkler[idx];
        let rgb = renkler[idx].match(/\w\w/g).map(x=>parseInt(x,16));
        let bright = getBrightness(rgb[0],rgb[1],rgb[2]);
        cells[asal].style.color = bright > 128 ? '#000' : '#fff';
      }
      document.getElementById('primeCount').innerText = document.querySelectorAll('.cell.prime,.cell.prime-gray').length;
    }
    else if(cmd === 'GRAYPRIME') {
      let asal = parseInt(p[1]);
      cells[asal].className = 'cell prime-gray';
      cells[asal].style.backgroundColor = '';
      document.getElementById('primeCount').innerText = document.querySelectorAll('.cell.prime,.cell.prime-gray').length;
    }
    else if(cmd === 'ELIM') {
      let sayi = parseInt(p[1]);
      let asal = parseInt(p[2]);
      let idx = asalIndeksMap[asal];
      if(idx !== undefined) {
        if(!hucreBolenMask[sayi]) hucreBolenMask[sayi] = 0;
        hucreBolenMask[sayi] |= (1 << idx);
      }
      cells[sayi].className = 'cell elim';
      hucreyiGuncelle(sayi);
      document.getElementById('elimCount').innerText = document.querySelectorAll('.cell.elim').length + ' / 100';
    }
    else if(cmd === 'ELIMMASK') {
      let sayi = parseInt(p[1]);
      let mask = parseInt(p[2]);
      hucreBolenMask[sayi] = mask;
      if(cells[sayi].classList.contains('elim')) hucreyiGuncelle(sayi);
    }
    else if(cmd === 'WIN') {
      asallariBuyut();
    }
    else if(cmd === 'RESET') {
      location.reload();
    }
  };
}
webSocketBaglan();
</script>
</body>
</html>
)=====";

// ---------- FONKSİYONLAR ----------
void TarayiciyaGonder(const String& komut, int sayi1, int sayi2 = -1) {
  String mesaj = komut + ":" + String(sayi1);
  if(sayi2 != -1) mesaj += ":" + String(sayi2);
  webSocket.broadcastTXT(mesaj);
}

void BaslangicDurumunuGonder() {
  for(int i=2; i<=100; i++){
    if(sayiDurumlari[i] == 2){
      int idx = asalIndeks[i];
      if(idx >= 0) TarayiciyaGonder("PRIME", i, idx);
    }
  }
  const int griAsallar[4] = {2,3,5,7};
  for(int i=0; i<4; i++){
    if(barSecildi[i]){
      TarayiciyaGonder("GRAYPRIME", griAsallar[i]);
    }
  }
  for(int i=2; i<=100; i++){
    if(sayiDurumlari[i] == 1 && sayiBolenMask[i] != 0){
      TarayiciyaGonder("ELIMMASK", i, sayiBolenMask[i]);
      TarayiciyaGonder("ELIM", i, 0);
    }
  }
  TarayiciyaGonder("BARPOS", barSecili);
  for(int i=0; i<4; i++) {
    TarayiciyaGonder("BARSELECT", i, barSecildi[i] ? 1 : 0);
  }
}

void oyunuSifirla() {
  memset(sayiDurumlari, 0, sizeof(sayiDurumlari));
  memset(sayiBolenMask, 0, sizeof(sayiBolenMask));
  memset(asalIndeks, -1, sizeof(asalIndeks));
  memset(barSecildi, 0, sizeof(barSecildi));
  sayiDurumlari[1] = 1;
  barSecili = 0;
  oyunBitti = false;
  oyunBitisZamani = 0;
  display.showNumberDec(2);
  TarayiciyaGonder("RESET", 0);
  delay(200);
  BaslangicDurumunuGonder();
}

void yilanAnimasyonu() {
  const uint8_t yilan[] = {
    0b00000001, 0b00000010, 0b00000100, 0b00001000,
    0b00010000, 0b00100000, 0b01000000, 0b00000000
  };
  for(int tur=0; tur<3; tur++) {
    for(int i=0; i<8; i++) {
      uint8_t segs[4] = {yilan[i], yilan[(i+1)%8], yilan[(i+2)%8], yilan[(i+3)%8]};
      display.setSegments(segs);
      delay(60);
      webSocket.loop();
    }
  }
  display.showNumberDec(0);
}

void oyunBittiAnimasyonu() {
  yilanAnimasyonu();
  delay(300);
  for(int i=2; i<=100; i++){
    if(sayiDurumlari[i] == 2){
      display.showNumberDec(i);
      delay(80);
      webSocket.loop();
    }
  }
  display.showNumberDec(0);
}

// ---------- SETUP ----------
void setup() {

  Wire.begin();
  Wire.setClock(100000);
  display.setBrightness(7);
  display.showNumberDec(2);

  pinMode(DAHILI_BUTON, INPUT_PULLUP);

  if(KumandaKolu.begin(0x1A)){
    joystickVar = true;
  }

  oyunuSifirla();

  WiFi.mode(WIFI_STA);
  WiFi.begin(evSSID, evSifre);
  int deneme = 0;
  while (WiFi.status() != WL_CONNECTED && deneme < 20) { delay(1000); deneme++; }
  if(WiFi.status() != WL_CONNECTED) {
    WiFi.mode(WIFI_AP);
    WiFi.softAP(apSSID, apSifre);
    display.showNumberDec(0);
  }

  server.begin();
  webSocket.begin();
  webSocket.onEvent([](uint8_t num, WStype_t type, uint8_t* payload, size_t len){
    if(type == WStype_CONNECTED){
      BaslangicDurumunuGonder();
    }
  });
}

// ---------- LOOP ----------
void loop() {
  webSocket.loop();

  if(digitalRead(DAHILI_BUTON) == LOW && oyunBitti) {
    if(millis() - sonDahiliButon > dahiliButonGecikmesi) {
      sonDahiliButon = millis();
      oyunuSifirla();
    }
  }

  if(millis() - sonJoyKontrol > gecikmeJoyKontrol) {
    sonJoyKontrol = millis();
    bool durum = KumandaKolu.begin(0x1A);
    if(durum != joystickVar) {
      joystickVar = durum;
    }
  }

  WiFiClient client = server.available();
  if(client){
    String line = "";
    while(client.connected()){
      if(client.available()){
        char c = client.read();
        if(c == '\n'){
          if(line.length() == 0){
            client.println("HTTP/1.1 200 OK");
            client.println("Content-type:text/html");
            client.println("Connection: close");
            client.println();
            client.print(HTML_INTERFACE);
            break;
          } else line = "";
        } else if(c != '\r') line += c;
      }
    }
    client.stop();
  }

  if(joystickVar && !oyunBitti){
    uint16_t xVal = KumandaKolu.xRead();
    bool buton = KumandaKolu.swRead();

    if(millis() - sonHareket > gecikmeHareket) {
      int eski = barSecili;
      if(xVal > ESIK_SAG && barSecili < 3) barSecili++;
      else if(xVal < ESIK_SOL && barSecili > 0) barSecili--;
      if(barSecili != eski) {
        sonHareket = millis();
        TarayiciyaGonder("BARPOS", barSecili);
        const int barSayilar[] = {2,3,5,7};
        display.showNumberDec(barSayilar[barSecili]);
      }
    }

    if(buton && (millis() - sonButon > gecikmeButon)){
      sonButon = millis();
      const int barSayilar[] = {2,3,5,7};
      int secilen = barSayilar[barSecili];

      if(!barSecildi[barSecili] && sayiDurumlari[secilen] == 0){
        barSecildi[barSecili] = true;
        TarayiciyaGonder("BARSELECT", barSecili, 1);

        sayiDurumlari[secilen] = 2;
        int idx = barSecili;
        asalIndeks[secilen] = idx;
        TarayiciyaGonder("PRIME", secilen, idx);
        TarayiciyaGonder("GRAYPRIME", secilen);

        for(int i = secilen*2; i <= 100; i += secilen){
          if(sayiDurumlari[i] != 2){
            if(sayiDurumlari[i] == 0) sayiDurumlari[i] = 1;
            sayiBolenMask[i] |= (1 << idx);
            TarayiciyaGonder("ELIM", i, secilen);
            display.showNumberDec(i);
            delay(50);
            webSocket.loop();
          }
        }
        display.showNumberDec(secilen);

        if(barSecildi[0] && barSecildi[1] && barSecildi[2] && barSecildi[3]) {
          oyunBitti = true;
          oyunBittiAnimasyonu();
          TarayiciyaGonder("WIN", 0);
        }
      }
    }
  }
  delay(1);
}