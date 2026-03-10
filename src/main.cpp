#include <Arduino.h>

#include "app/AppController.h"

namespace {

sharp::AppController app;

}  // namespace

void setup() { app.setup(); }

void loop() { app.loop(); }
