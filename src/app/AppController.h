#pragma once

namespace sharp {

class AppController {
 public:
  AppController();
  ~AppController();

  void setup();
  void loop();

 private:
  class Impl;
  void handleMqttCallback(char* topic, unsigned char* payload, unsigned int length);
  Impl* impl_ = nullptr;
};

}  // namespace sharp
