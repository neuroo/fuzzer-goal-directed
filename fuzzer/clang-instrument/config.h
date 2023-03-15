#ifndef CONFIG_H
#define CONFIG_H

struct Config {
  int foo;

  Config(){}
  Config(const Config& c){
    foo = c.foo;
  }
  Config& operator=(const Config& c) {
    return *this;
  }
};


#endif
