#include <RobustQuadrature.h>

RobustQuadrature::One<2, 3> encoder;

int pos0;

void setup() {
  Serial.begin(115200);
  while (!Serial) {
  }
  Serial.println("RobustQuadrature demo");
  pos0 = 0;
}

void loop() {
  int pos = encoder.position();
  if (pos != pos0) {
    Serial.println(pos);
    pos0 = pos;
  }
}
