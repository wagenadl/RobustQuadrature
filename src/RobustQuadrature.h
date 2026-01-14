// RobustQuadrature.h
// A robust reader for quadrature encoders

#include <limits.h>
#include <stdint.h>

namespace RobustQuadrature {
  // these are the main state for any encoder
  volatile int32_t check = 0;
  template <int pina, int pinb> volatile int32_t counter;
  template <int pina, int pinb> signed char stateA;
  template <int pina, int pinb> signed char stateB;
  template <int pina, int pinb> void (*callback)(int32_t);

  // these are only for two and four steps per cycle
  template <int pina, int pinb> signed char putativestateA;
  template <int pina, int pinb> signed char putativestateB;
  template <int pina, int pinb> unsigned long lastA;
  template <int pina, int pinb> unsigned long lastB;
  template <int pina, int pinb> unsigned long holdoff;

  // ISRs for one step per cycle
  template <int pina, int pinb> void isrA1() {
    stateB<pina, pinb> = digitalRead(pinb) ? 1 : -1;
    if (stateA<pina, pinb> > 0) {
      counter<pina, pinb> += stateB<pina, pinb>;
      if (callback<pina, pinb>)
        callback<pina, pinb>(counter<pina, pinb>);
    }
    stateA<pina, pinb> = 0;
  }

  template <int pina, int pinb> void isrB1() {
    stateA<pina, pinb> = digitalRead(pina) ? 1 : -1;
    stateB<pina, pinb> = 0;
  }

  template <int pina, int pinb> void isrA1x() {
    // this version uses holdoff
    stateB<pina, pinb> = digitalRead(pinb) ? 1 : -1;
    unsigned long t = micros();
    if (stateA<pina, pinb> == 0) 
      if (holdoff<pina, pinb>
          && t - lastA<pina, pinb> >= holdoff<pina, pinb>)
        stateA<pina, pinb> = putativestateA<pina, pinb>;
    if (stateA<pina, pinb>) {
      if (stateA<pina, pinb> > 0) {
        counter<pina, pinb> += stateB<pina, pinb>;
        if (callback<pina, pinb>)
          callback<pina, pinb>(counter<pina, pinb>);
      }
      putativestateA<pina, pinb> = -stateA<pina, pinb>;
      stateA<pina, pinb> = 0;
    }
    lastA<pina, pinb> = t;
  }
      
  // modified ISR for two steps per cycle
  template <int pina, int pinb> void isrA2() {
    stateB<pina, pinb> = digitalRead(pinb) ? 1 : -1;
    unsigned long t = micros();
    if (stateA<pina, pinb> == 0) 
      if (holdoff<pina, pinb>
          && t - lastA<pina, pinb> >= holdoff<pina, pinb>)
        stateA<pina, pinb> = putativestateA<pina, pinb>;
    if (stateA<pina, pinb>) {
      counter<pina, pinb> += stateA<pina, pinb> > 0
        ? stateB<pina, pinb>
        : -stateB<pina, pinb>;
      if (callback<pina, pinb>)
        callback<pina, pinb>(counter<pina, pinb>);
      putativestateA<pina, pinb> = -stateA<pina, pinb>;
      stateA<pina, pinb> = 0;
    }
    lastA<pina, pinb> = t;
  }
      
  // isrB2 is no different from isrB1

  // modified ISR for four steps per cycle

  // isrA4 is no different from isrA2
  
  template <int pina, int pinb> void isrB4() {
    stateA<pina, pinb> = digitalRead(pina) ? 1 : -1;
    unsigned long t = micros();
    if (stateB<pina, pinb> == 0) 
      if (holdoff<pina, pinb>
          && t - lastB<pina, pinb> >= holdoff<pina, pinb>)
        stateB<pina, pinb> = putativestateB<pina, pinb>;
    if (stateB<pina, pinb>) {
      counter<pina, pinb> += stateB<pina, pinb> > 0
        ? -stateA<pina, pinb>
        : stateA<pina, pinb>;
      if (callback<pina, pinb>)
        callback<pina, pinb>(counter<pina, pinb>);
      putativestateB<pina, pinb> = -stateB<pina, pinb>;
      stateB<pina, pinb> = 0;
    }
    lastB<pina, pinb> = t;
  }

  template <int pina, int pinb> bool valid() {
    return digitalPinToInterrupt(pina) >= 0 && digitalPinToInterrupt(pinb) >= 0;
  }


  template <int pina, int pinb> class Base {
  public:
    Base() {
      RobustQuadrature::callback<pina, pinb> = 0;
      counter<pina, pinb> = 0;
      holdoff<pina, pinb> = 0;
      pinMode(pina, INPUT_PULLUP);
      pinMode(pinb, INPUT_PULLUP);
      stateA<pina, pinb> = digitalRead(pina) ? 1 : -1;
      stateA<pina, pinb> = digitalRead(pinb) ? 1 : -1;
    }

    bool valid() const {
      return RobustQuadrature::valid<pina, pinb>();
    }
    
    void setHoldoff(unsigned long holdoff_us) {
      holdoff<pina, pinb> = holdoff_us;
    }
    
    void setCallback(void (*callback)(int32_t)) {
      RobustQuadrature::callback<pina, pinb> = callback;
    }
    
    int32_t position() const {
#if SIG_ATOMIC_MAX < INT32_MAX
      // On CPUs with fewer than 32 bits native type, we
      // need to be careful when accessing the counter.
      noInterrupts();
      int32_t x = counter<pina, pinb>;
      interrupts();
      return x;
#else
      // If 32-bit read is atomic, we won't bother turning interrupts off.
      return counter<pina, pinb>;
#endif
    }
  };

  //////////////////////////////////////////////////////////////////////
  // Public classes
  
  template <int pina, int pinb> class One: public Base<pina, pinb> {
  public:
    One() {
      attachInterrupt(digitalPinToInterrupt(pina), isrA1<pina, pinb>, CHANGE);
      attachInterrupt(digitalPinToInterrupt(pinb), isrB1<pina, pinb>, CHANGE);
    }
    void setHoldoff(unsigned long holdoff_us) {
      holdoff<pina, pinb> = holdoff_us;
      detachInterrupt(digitalPinToInterrupt(pina));
      attachInterrupt(digitalPinToInterrupt(pina), isrA1x<pina, pinb>, CHANGE);
    }
  };

  template <int pina, int pinb> class Two: public Base<pina, pinb> {
  public:
    Two() {
      holdoff<pina, pinb> = 1000;
      attachInterrupt(digitalPinToInterrupt(pina), isrA2<pina, pinb>, CHANGE);
      attachInterrupt(digitalPinToInterrupt(pinb), isrB1<pina, pinb>, CHANGE);
    }
  };

  template <int pina, int pinb> class Four: public Base<pina, pinb> {
  public:
    Four() {
      holdoff<pina, pinb> = 1000;
      attachInterrupt(digitalPinToInterrupt(pina), isrA2<pina, pinb>, CHANGE);
      attachInterrupt(digitalPinToInterrupt(pinb), isrB4<pina, pinb>, CHANGE);
    }
  };
}
