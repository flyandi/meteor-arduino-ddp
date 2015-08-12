/*
  Simple Meteor DDP Implemention for Arduino
  
  https://github.com/meteor/meteor/blob/devel/packages/ddp/DDP.md
*/


#include <ArduinoJson.h>
#include "config.h"
#include "ddp.h"

/**
 * Globals
 */

StaticJsonBuffer<METEOR_DATABASE_SIZE> _collectionBuffer;  // 5kb database

JsonObject& _collection = _collectionBuffer.createObject();

/**
 * (DDP)
 */

DDP::DDP() {
  
}


/**
 *  (setup) setups the connection 
 */

void DDP::setup(String host) {
  
  // assign data
  _host = host;
  _port = METEOR_PORT;
  _path = METEOR_PATH;

}


/**
 * (connect) connects to the server
 */

bool DDP::connect() {
  
  bool connected = false;
    
  // Connect to the websocket server
  if(wifiClient.connect(_host.c_str(), _port)) {
    #ifdef DDP_DEBUG
      _debug("Connected to server.");
    #endif

    // handshake
    if(_handshake()) {
      connected = _acquire();
    }
  
  } else {
    #ifdef DDP_DEBUG
      _debug("Connection to server failed.");
    #endif
  }
  
  return connected;
}


/**
 * (_handshake) executes a handshake between the server and the arduino
 */

bool DDP::_handshake() {

  // prepare socket client
  socketClient.host = strdup(_host.c_str());
  socketClient.path = strdup(_path.c_str());

  // socket handshake
  if (socketClient.handshake(wifiClient)) {
    #ifdef DDP_DEBUG
      _debug("Handshake successful.");
    #endif
    
    return true;

  } 

  #ifdef DDP_DEBUG
    _debug("Handshake failed.");
  #endif

  return false;
}


/*
 * (_acquire) gets the final connection
 */
bool DDP::_acquire() {

  // Prepare message
  StaticJsonBuffer<200> _jsonBuffer;
  JsonObject& root = _jsonBuffer.createObject();
  root["msg"]     = "connect";
  root["version"] = "1";

  JsonArray& support = _jsonBuffer.createArray();
  support.add("1");
  root["support"] = support;

  char buffer[200];
  root.printTo(buffer, sizeof(buffer));

  // Send message
  socketClient.sendData(buffer);
  delay(_pause);
  
  // handle response
  String response;
  socketClient.getData(response);   

  if (response.length() > 0) {
    #ifdef DDP_DEBUG
      _debug("Received: response");
      _debug(response);
    #endif
  }

  // handle status
  bool status = false;

  if (response.indexOf("failed") >= 0) {
    status = false;
  } else if (response.indexOf("connected") >= 0) {
    status = true;
    // {"msg":"connected","session":"zwKbMXqs7jcKrke4Y"}
    _session = response.substring(30, 47); // static definition
  } 
  
  return status;

  /* TODO Use parseObject
  root = _jsonBuffer.parseObject(response);
  if (!root.success()) {
    Serial.println("parseObject() failed");
    return;
  }

  char* msg = root["msg"];
  String msgs(msg);

  if (msgs.equals("failed")) {
    Serial.println("failed");
  } else if (msgs.equals("connected")) {
    Serial.println("connected");
  }
  */
}


/*
 * (process)
 */

void DDP::process() {

  #ifdef DDP_DEBUG
    if(DDP_DEBUG == 2) 
      _debug("processing");
  #endif


  // Check if wificlient is connected
  while(wifiClient.connected()) {

    // process heartbeat
    if (_timer % 40 == 0) {
      ping();
    }

    // safety exit
    if (_timer % 10 == 0) {
      _timer++;

      #ifdef DDP_DEBUG
        if(DDP_DEBUG == 2)
          _debug("Return because timer");
      #endif

      return;
    }

    // process data
    String data;
    socketClient.getData(data);

    // check data
    if (data.length() == 0) {
    
      #ifdef DDP_DEBUG
        if(DDP_DEBUG == 2) 
          _debug("Continue because no data");
      #endif

      delay(_pause);
      _timer++;
      
      return;
    }

    // write data
    #ifdef DDP_DEBUG
      _debug("------------------------------");
      _debug(data);
      _debug("------------------------------");
    #endif

    // create temporary buffer and parse json
    StaticJsonBuffer<512> jsonBuffer;
    JsonObject& root = jsonBuffer.parseObject(strdup(data.c_str()));

    if(!root.success()) {

      #ifdef DDP_DEBUG
        _debug("Invalid JSON.");
      #endif

      return;
    }


    /**
     * process messages
     */

    String msg = root["msg"].asString();

    /*
     * Heartbeats
     */

    if(msg == "ping") {

      #ifdef DDP_DEBUG 
        _debug("Received: ping");
        _debug(root["id"].asString());
      #endif

      // pong
      pong(root["id"].asString());

      continue;
    }

    // Pong
    if(msg == "pong") {

      #ifdef DDP_DEBUG
        _debug("Received: pong");
      #endif

      continue;
    }


    /*
     * Data Management
     */ 

    if(msg == "nosub") {

      #ifdef DDP_DEBUG
        _debug("Received: nosub");
        _debug(root["error"].asString());
      #endif

      continue;
    }

    if(msg == "added" || msg == "changed") {

      bool isChanged = msg == "changed";

      // process collection
      #ifdef DDP_DEBUG
        if(isChanged) {
          _debug("Received: changed");
        } else {
          _debug("Received: added");
        }
        _debug(root["collection"].asString());
      #endif

      // parse data
      const char* name = root["collection"].asString();
      
      // check if collection exists
      if(_collection.containsKey(name)) {

        const char* id = root["id"].asString();
        

        // found registered collection, adding id to collection
        if(isChanged) {

          // check if id exists
          if(_collection[name].asObject().containsKey(id)) {

            for(JsonObject::iterator it=root["fields"].asObject().begin(); it!=root["fields"].asObject().end(); ++it) {
            
              if(_collection[name].asObject()[id].asObject().containsKey(it->key)) {
                _collection[name][id][it->key] = it->value; 
              }

            }
            
          } else {    
          
            JsonObject& param = _collection[name].asObject().createNestedObject(root["id"].asString());
       
            for(JsonObject::iterator it=root["fields"].asObject().begin(); it!=root["fields"].asObject().end(); ++it) {
              param[it->key] = it->value;
            }

          }
        
        }

      }

      continue;
    }


    if(msg == "ready") {

      #ifdef DDP_DEBUG
        _debug("Received: ready");
        _debug(root["id"].asString());
      #endif

      //{"msg":"ready","subs":["1"]}

      continue;

    }


    if(msg == "result") {

      #ifdef DDP_DEBUG
        _debug("Received: result");
        _debug(root["id"].asString());
      #endif


      // check for error
      if(root.containsKey("error")) {

        #ifdef DDP_DEBUG
          _debug("Result has error");
          _debug(root["error"].asString());
        #endif

        // notify error
      
      }

      // check for result
      if(root.containsKey("result")) {

        #ifdef DDP_DEBUG
          _debug("Result has result");
          _debug(root["result"].asString());
        #endif

        // notify result
        _notifyCallback(root["id"].asString(), root["result"].asObject());

      }

    }


    /** 
     * RPC Handling
     */

    if(msg == "update") {
      #ifdef DDP_DEBUG
        _debug("Received: update");
        _debug(root["id"].asString());
      #endif

      // methods
      //_handledUpdated = true;
    }

    
    delay(_pause); // processing brake
  
  } /* eof while */
  
} /* eof process */


/**
 * Heartbeats
 */

void DDP::ping(String id) {

  #ifdef DDP_DEBUG
    _debug("Sending: ping");
    _debug(id);
  #endif

  StaticJsonBuffer<200> _jsonBuffer;
  JsonObject& root = _jsonBuffer.createObject();
  root["msg"] = "ping";

  if (id.length() > 0) {
    root["id"] = id;
  }

  char buffer[200];
  root.printTo(buffer, sizeof(buffer));

  socketClient.sendData(buffer);
}


void DDP::pong(String id) {

  #ifdef DDP_DEBUG
    _debug("Sending: pong");
    _debug(id);
  #endif

  StaticJsonBuffer<200> _jsonBuffer;
  JsonObject& root = _jsonBuffer.createObject();
  root["msg"] = "pong";

  if (id.length() > 0) {
    root["id"] = id;
  }

  char buffer[200];
  root.printTo(buffer, sizeof(buffer));

  socketClient.sendData(buffer);
}


/** 
 * Flow Management
 */

bool DDP::subscribe(String name, void(*)(JsonObject& data)) {

  StaticJsonBuffer<200> _jsonBuffer;
  JsonObject& root = _jsonBuffer.createObject();
  root["msg"] = "sub";
  root["id"] = _subscriptionIndex;
  root["name"] = name;

  char buffer[200];
  root.printTo(buffer, sizeof(buffer));
  socketClient.sendData(buffer);

  delay(_pause);

  _subscriptionIndex++;

  // register callback


  return true;
}


bool DDP::call(String name, JsonArray& params) {

  // build message
  StaticJsonBuffer<512> _jsonBuffer;
  JsonObject& root = _jsonBuffer.createObject();
  root["msg"] = "method";
  root["method"] = name.c_str();
  root["params"] = params;
  root["id"] = name.c_str();

  // set buffer
  char buffer[512];
  root.printTo(buffer, sizeof(buffer));

  #ifdef DDP_DEBUG
    _debug("Sending: call method");
    _debug(name);
    _debug(buffer);
  #endif

  socketClient.sendData(buffer);

  return true;
}

bool DDP::call(String name, String key, String value) {
  
  StaticJsonBuffer<200> _buffer;
  JsonArray& params = _buffer.createArray();
  JsonObject& values = _buffer.createObject();
  values.add(key.c_str(), value.c_str());
  params.add(values);

  call(name, params);
}

bool DDP::call(String name, String key, long value) {
  StaticJsonBuffer<200> _buffer;
  JsonArray& params = _buffer.createArray();
  JsonObject& values = _buffer.createObject();
  values.add(key.c_str(), value);
  params.add(values);

  call(name, params);
}

bool DDP::call(String name, String key, long value, void (*function)(JsonObject&)) {

  if(_registerCallback(name, function, true)) {
    return call(name, key, value);
  } 

  return false;  
}


/**
 * Privates
 */

#ifdef DDP_DEBUG
  void DDP::_debug(String s) {
     Serial.println(s);
  }
  void DDP::_debug(byte s) {
     Serial.println(s);
  }
#endif

bool DDP::_registerCollection(String name) {
  // check if collection already exists
  if(!_collection.containsKey(name.c_str())) {
    // create new collection
    _collection.createNestedObject(name.c_str());

    #ifdef DDP_DEBUG
      _debug("Collection: adding");
      _debug(name);
    #endif

    return true;
  }

  return false;
}

bool DDP::_registerCallback(String name, void (*function)(JsonObject&), bool isMethod) {

  int index = -1;

  // find available index and check for potential waiting method
  
  for(byte i = 0; i < _callbackCount; i++) {

    if(_callbackList[i].used) {

      // check if it's a method with the same name
      if(isMethod && _callbackList[i].methodCall && _callbackList[i].name == name) {
        // clear timeout
        if(_callbackList[i].timeout % 10 == 0) {
          index = i; // make it available
        } else {
          _callbackList[i].timeout++; // update timeout and block
        }
        break;
      }
    }

    // check for an empty slot
    if(!_callbackList[i].used) {
      index = i;
      break;
    }
  }

  // register callback only if doesn't exceed callback amount
  if(_callbackCount < DDP_MAX_CALLBACKS) {

    // get the correct index
    if(index == -1) {
       index = _callbackCount;
       _callbackCount++;
    }

    _callbackList[index].name = name;
    _callbackList[index].function = function;
    _callbackList[index].used = true;
    _callbackList[index].methodCall = isMethod;
    _callbackList[index].timeout = 0;

    #ifdef DDP_DEBUG
      _debug("Callback: register");
      _debug(name);
      _debug(index);
    #endif

    return true;
  } 

  #ifdef DDP_DEBUG
      _debug("Callback: too many callbacks");
      _debug(name);
  #endif

  return false;
}

bool DDP::_notifyCallback(String name, JsonObject& result) {

  // find callback
  for(byte i = 0; i < _callbackCount; i++) {
    
    if(_callbackList[i].methodCall && _callbackList[i].used && _callbackList[i].name == name) {
      // notify
      _callbackList[i].function(result);
      
      // clear
      _callbackList[i].used = false;
    }
  }
}
  
