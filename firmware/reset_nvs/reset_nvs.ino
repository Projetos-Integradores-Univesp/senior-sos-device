#include <Preferences.h>
void setup() {
    Preferences p;
    p.begin("elderguard", false);
    p.clear();
    p.end();
    Serial.begin(115200);
    Serial.println("NVS limpo — regrave o firmware ElderGuard");
}
void loop() {}