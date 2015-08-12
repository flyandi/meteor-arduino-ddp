/*
  DDP library for Arduino
*/

#ifndef _DDP_h
#define _DDP_h

#include <Strings.h>
#include <ArduinoJson.h>

/**
 * Definitions
 */

#ifndef METEOR_PATH
  #define METEOR_PATH "/websocket"
#endif

#ifndef METEOR_PORT
  #define METEOR_PORT 3000
#endif

#ifndef METEOR_DATABASE_SIZE
  #define METEOR_DATABASE_SIZE 5120 // 5kb
#endif

#ifndef DDP_MAX_CALLBACKS
  #define DDP_MAX_CALLBACKS 5
#endif

/**
 * Debug Mode  
 * Uncomment to disable
 * 1 = Data messages only
 * 2 = All messages
 */
 
//#define DDP_DEBUG 1


/**
 * (Class) DDP
 */

class DDP {
  public:
    /**
     * Constructor
     */

    DDP();

    /* 
     * Connection
     */

    bool 
        connect();

    void
        setup(String host),
        process();

    /*
     * Heartbeats
     */
    
    void 
        ping(String id = ""),
        pong(String id = "");


    /*
     * Flow Management
     */

   
    bool        
        // (subscribe) to an collection
        subscribe(String collection, void(*)(JsonObject& data)),

        // (call) Calls a method on the server
        call(String name, JsonArray& params),
        call(String name, String key, String value),
        call(String name, String key, long value),
        call(String name, String key, long value, void (*function)(JsonObject&));
    
    
  private:

    /**
     * Internals
     */

    bool
        _handshake(),
        _acquire(),
        _acquireServer(),
        _registerCollection(String name),
        _registerCallback(String name, void (*function)(JsonObject&), bool isMethod),
        _notifyCallback(String name, JsonObject& result);


    /** 
     * Debug
     */

    #ifdef DDP_DEBUG
      void 
        _debug(String s),
        _debug(int s);
    #endif
   

    /** 
     * Locals
     */

    int 
        _pause = 100,
        _timer = 1,
        _port = 3000,
        _subscriptionIndex = 1;

    String
        _host,
        _path,
        _session;
    

    typedef struct _callback {
        bool used = false,
             methodCall = false;
        byte timeout = 0;
        String name;
        void (*function)(JsonObject&);
    } DDPCallback; 

    int _callbackCount = 0;
    
    DDPCallback _callbackList[DDP_MAX_CALLBACKS]; 
};


#endif /* _DDP_h */
