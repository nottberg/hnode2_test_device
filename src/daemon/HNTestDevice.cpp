#include <unistd.h>
#include <string.h>
#include <syslog.h>

#include <iostream>
#include <sstream>
#include <thread>

#include "Poco/Util/ServerApplication.h"
#include "Poco/Util/Option.h"
#include "Poco/Util/OptionSet.h"
#include "Poco/Util/HelpFormatter.h"
#include "Poco/Checksum.h"
#include <Poco/JSON/Object.h>
#include <Poco/JSON/Parser.h>
#include <Poco/StreamCopier.h>

#include <hnode2/HNodeDevice.h>

#include "HNTestDevicePrivate.h"

using namespace Poco::Util;

namespace pjs = Poco::JSON;
namespace pdy = Poco::Dynamic;

// Forward declaration
extern const std::string g_HNode2TestRest;

void 
HNTestDevice::defineOptions( OptionSet& options )
{
    ServerApplication::defineOptions( options );

    options.addOption(
              Option("help", "h", "display help").required(false).repeatable(false));

    options.addOption(
              Option("debug","d", "Enable debug logging").required(false).repeatable(false));

    options.addOption(
              Option("instance", "", "Specify the instance name of this daemon.").required(false).repeatable(false).argument("name"));

}

void 
HNTestDevice::handleOption( const std::string& name, const std::string& value )
{
    ServerApplication::handleOption( name, value );
    if( "help" == name )
        _helpRequested = true;
    else if( "debug" == name )
        _debugLogging = true;
    else if( "instance" == name )
    {
         _instancePresent = true;
         _instance = value;
    }
}

void 
HNTestDevice::displayHelp()
{
    HelpFormatter helpFormatter(options());
    helpFormatter.setCommand(commandName());
    helpFormatter.setUsage("[options]");
    helpFormatter.setHeader("HNode2 Switch Daemon.");
    helpFormatter.format(std::cout);
}

int 
HNTestDevice::main( const std::vector<std::string>& args )
{
    m_instanceName = "default";
    if( _instancePresent == true )
        m_instanceName = _instance;

    m_hnodeDev.setDeviceType( HNODE_TEST_DEVTYPE );
    m_hnodeDev.setInstance( m_instanceName );

    HNDEndpoint hndEP;

    hndEP.setDispatch( "hnode2Test", this );
    hndEP.setOpenAPIJson( g_HNode2TestRest ); 

    m_hnodeDev.addEndpoint( hndEP );

    m_hnodeDev.setRestPort(8088);

    std::cout << "Looking for config file" << std::endl;
    
    if( configExists() == false )
    {
        initConfig();
    }

    readConfig();

    // Setup the event loop
    m_testDeviceEvLoop.setup( this );

    m_testDeviceEvLoop.setupTriggerFD( m_configUpdateTrigger );

    // Register some format strings
    m_hnodeDev.registerFormatString( "Error: %u", m_errStrCode );
    m_hnodeDev.registerFormatString( "This is a test note.", m_noteStrCode );

    // Enable the health monitoring and add some fake components
    m_hnodeDev.enableHealthMonitoring();

    m_hnodeDev.getHealthRef().registerComponent( "test device hc1", HNDH_ROOT_COMPID, m_hc1ID );
    std::cout << "Health Component 1 id: " << m_hc1ID << std::endl;
    m_hnodeDev.getHealthRef().registerComponent( "test device hc2", HNDH_ROOT_COMPID, m_hc2ID );
    std::cout << "Health Component 2 id: " << m_hc2ID << std::endl;
    m_hnodeDev.getHealthRef().registerComponent( "test device hc2.1", m_hc2ID, m_hc3ID );
    std::cout << "Health Component 3 id: " << m_hc3ID << std::endl;

    m_healthStateSeq = 0;
    generateNewHealthState();

    // Start accepting device notifications
    m_hnodeDev.setNotifySink( this );

    // Start up the hnode device
    m_hnodeDev.start();

    // Start event processing loop
    m_testDeviceEvLoop.run();

    waitForTerminationRequest();

    return Application::EXIT_OK;
}

void
HNTestDevice::generateNewHealthState()
{
    m_hnodeDev.getHealthRef().startUpdateCycle( time(NULL) );

    switch( m_healthStateSeq )
    {
        case 0:
          m_hnodeDev.getHealthRef().setComponentStatus( HNDH_ROOT_COMPID, HNDH_CSTAT_OK );
          m_hnodeDev.getHealthRef().setComponentStatus( m_hc1ID, HNDH_CSTAT_OK );
          m_hnodeDev.getHealthRef().setComponentStatus( m_hc2ID, HNDH_CSTAT_OK );
          m_hnodeDev.getHealthRef().setComponentStatus( m_hc3ID, HNDH_CSTAT_OK );
        break;

        case 1:
          m_hnodeDev.getHealthRef().setComponentStatus( HNDH_ROOT_COMPID, HNDH_CSTAT_OK );

          m_hnodeDev.getHealthRef().setComponentStatus( m_hc1ID, HNDH_CSTAT_FAILED );
          m_hnodeDev.getHealthRef().setComponentErrMsg( m_hc1ID, 200, m_errStrCode, 200 );
          
          m_hnodeDev.getHealthRef().setComponentStatus( m_hc2ID, HNDH_CSTAT_OK );
          m_hnodeDev.getHealthRef().setComponentStatus( m_hc3ID, HNDH_CSTAT_OK );
        break;

        case 2:
          m_hnodeDev.getHealthRef().setComponentStatus( HNDH_ROOT_COMPID, HNDH_CSTAT_OK );

          m_hnodeDev.getHealthRef().setComponentStatus( m_hc1ID, HNDH_CSTAT_OK );
          m_hnodeDev.getHealthRef().clearComponentErrMsg( m_hc1ID );

          m_hnodeDev.getHealthRef().setComponentStatus( m_hc2ID, HNDH_CSTAT_OK );

          m_hnodeDev.getHealthRef().setComponentStatus( m_hc3ID, HNDH_CSTAT_FAILED );
          m_hnodeDev.getHealthRef().setComponentErrMsg( m_hc3ID, 400, m_errStrCode, 400 );
        break;

        case 3:
          m_hnodeDev.getHealthRef().setComponentStatus( HNDH_ROOT_COMPID, HNDH_CSTAT_OK );
          m_hnodeDev.getHealthRef().setComponentStatus( m_hc1ID, HNDH_CSTAT_OK );

          m_hnodeDev.getHealthRef().setComponentStatus( m_hc2ID, HNDH_CSTAT_OK );
          m_hnodeDev.getHealthRef().setComponentNote( m_hc2ID, m_noteStrCode );

          m_hnodeDev.getHealthRef().setComponentStatus( m_hc3ID, HNDH_CSTAT_OK );
          m_hnodeDev.getHealthRef().clearComponentErrMsg( m_hc3ID );
        break;

        case 4:
          m_hnodeDev.getHealthRef().setComponentStatus( HNDH_ROOT_COMPID, HNDH_CSTAT_OK );
          m_hnodeDev.getHealthRef().setComponentStatus( m_hc1ID, HNDH_CSTAT_OK );
          m_hnodeDev.getHealthRef().setComponentStatus( m_hc2ID, HNDH_CSTAT_OK );
          m_hnodeDev.getHealthRef().clearComponentNote( m_hc2ID );
          m_hnodeDev.getHealthRef().setComponentStatus( m_hc3ID, HNDH_CSTAT_OK );
        break;
    }

    // Advance to next simulated health state.
    m_healthStateSeq += 1;
    if( m_healthStateSeq > 4 )
      m_healthStateSeq = 0;

    bool changed = m_hnodeDev.getHealthRef().completeUpdateCycle();
} 

bool 
HNTestDevice::configExists()
{
    HNodeConfigFile cfgFile;

    return cfgFile.configExists( HNODE_TEST_DEVTYPE, m_instanceName );
}

HNTD_RESULT_T
HNTestDevice::initConfig()
{
    HNodeConfigFile cfgFile;
    HNodeConfig     cfg;

    m_hnodeDev.initConfigSections( cfg );

    cfg.debugPrint(2);
    
    std::cout << "Saving config..." << std::endl;
    if( cfgFile.saveConfig( HNODE_TEST_DEVTYPE, m_instanceName, cfg ) != HNC_RESULT_SUCCESS )
    {
        std::cout << "ERROR: Could not save initial configuration." << std::endl;
        return HNTD_RESULT_FAILURE;
    }

    return HNTD_RESULT_SUCCESS;
}

HNTD_RESULT_T
HNTestDevice::readConfig()
{
    HNodeConfigFile cfgFile;
    HNodeConfig     cfg;

    if( configExists() == false )
        return HNTD_RESULT_FAILURE;

    std::cout << "Loading config..." << std::endl;

    if( cfgFile.loadConfig( HNODE_TEST_DEVTYPE, m_instanceName, cfg ) != HNC_RESULT_SUCCESS )
    {
        std::cout << "ERROR: Could not load saved configuration." << std::endl;
        return HNTD_RESULT_FAILURE;
    }
  
    std::cout << "cl1" << std::endl;
    m_hnodeDev.readConfigSections( cfg );

    std::cout << "Config loaded" << std::endl;

    return HNTD_RESULT_SUCCESS;
}

HNTD_RESULT_T
HNTestDevice::updateConfig()
{
    HNodeConfigFile cfgFile;
    HNodeConfig     cfg;

    m_hnodeDev.updateConfigSections( cfg );

    cfg.debugPrint(2);
    
    std::cout << "Saving config..." << std::endl;
    if( cfgFile.saveConfig( HNODE_TEST_DEVTYPE, m_instanceName, cfg ) != HNC_RESULT_SUCCESS )
    {
        std::cout << "ERROR: Could not save configuration." << std::endl;
        return HNTD_RESULT_FAILURE;
    }
    std::cout << "Config saved" << std::endl;

    return HNTD_RESULT_SUCCESS;
}

void
HNTestDevice::loopIteration()
{
    // std::cout << "HNManagementDevice::loopIteration() - entry" << std::endl;

}

void
HNTestDevice::timeoutEvent()
{
    // std::cout << "HNManagementDevice::timeoutEvent() - entry" << std::endl;

}

void
HNTestDevice::fdEvent( int sfd )
{
    std::cout << "HNManagementDevice::fdEvent() - entry: " << sfd << std::endl;

    if( m_configUpdateTrigger.isMatch( sfd ) )
    {
        std::cout << "m_configUpdateTrigger - updating config" << std::endl;
        m_configUpdateTrigger.reset();
        updateConfig();
    }
}

void
HNTestDevice::fdError( int sfd )
{
    std::cout << "HNManagementDevice::fdError() - entry: " << sfd << std::endl;

}

void
HNTestDevice::hndnConfigChange( HNodeDevice *parent )
{
    std::cout << "HNManagementDevice::hndnConfigChange() - entry" << std::endl;
    m_configUpdateTrigger.trigger();
}

void 
HNTestDevice::dispatchEP( HNodeDevice *parent, HNOperationData *opData )
{
    std::cout << "HNTestDevice::dispatchEP() - entry" << std::endl;
    std::cout << "  dispatchID: " << opData->getDispatchID() << std::endl;
    std::cout << "  opID: " << opData->getOpID() << std::endl;
    std::cout << "  thread: " << std::this_thread::get_id() << std::endl;

    std::string opID = opData->getOpID();
          
    // GET "/hnode2/test/status"
    if( "getStatus" == opID )
    {
        std::cout << "=== Get Status Request ===" << std::endl;
    
        // Set response content type
        opData->responseSetChunkedTransferEncoding( true );
        opData->responseSetContentType( "application/json" );

        // Create a json status object
        pjs::Object jsRoot;
        jsRoot.set( "overallStatus", "OK" );

        // Render response content
        std::ostream& ostr = opData->responseSend();
        try{ 
            pjs::Stringifier::stringify( jsRoot, ostr, 1 ); 
        } catch( ... ) {
            std::cout << "ERROR: Exception while serializing comment" << std::endl;
        }

        // Request was successful
        opData->responseSetStatusAndReason( HNR_HTTP_OK );
    }
    // GET "/hnode2/test/widgets"
    else if( "getWidgetList" == opID )
    {
        std::cout << "=== Get Widget List Request ===" << std::endl;

        // Set response content type
        opData->responseSetChunkedTransferEncoding( true );
        opData->responseSetContentType( "application/json" );

        // Create a json root object
        pjs::Array jsRoot;

        pjs::Object w1Obj;
        w1Obj.set( "id", "w1" );
        w1Obj.set( "color", "red" );
        jsRoot.add( w1Obj );

        pjs::Object w2Obj;
        w2Obj.set( "id", "w2" );
        w2Obj.set( "color", "green" );
        jsRoot.add( w2Obj );

        pjs::Object w3Obj;
        w3Obj.set( "id", "w3" );
        w3Obj.set( "color", "blue" );
        jsRoot.add( w3Obj );
          
        // Render response content
        std::ostream& ostr = opData->responseSend();
        try{ 
            pjs::Stringifier::stringify( jsRoot, ostr, 1 ); 
        } catch( ... ) {
            std::cout << "ERROR: Exception while serializing comment" << std::endl;
        }
            
        // Request was successful
        opData->responseSetStatusAndReason( HNR_HTTP_OK );
    }
    // GET "/hnode2/test/widgets/{widgetid}"
    else if( "getWidgetInfo" == opID )
    {
        std::string widgetID;

        if( opData->getParam( "widgetid", widgetID ) == true )
        {
            opData->responseSetStatusAndReason( HNR_HTTP_INTERNAL_SERVER_ERROR );
            opData->responseSend();
            return; 
        }

        std::cout << "=== Get Widget Info Request (id: " << widgetID << ") ===" << std::endl;

        // Set response content type
        opData->responseSetChunkedTransferEncoding( true );
        opData->responseSetContentType( "application/json" );
        
        // Create a json root object
        pjs::Array jsRoot;

        pjs::Object w1Obj;
        w1Obj.set( "id", widgetID );
        w1Obj.set( "color", "black" );
        jsRoot.add( w1Obj );
          
        // Render response content
        std::ostream& ostr = opData->responseSend();
        try{ 
            pjs::Stringifier::stringify( jsRoot, ostr, 1 ); 
        } catch( ... ) {
            std::cout << "ERROR: Exception while serializing comment" << std::endl;
        }            
        // Request was successful
        opData->responseSetStatusAndReason( HNR_HTTP_OK );

    }
    // POST "/hnode2/test/widgets"
    else if( "createWidget" == opID )
    {
        std::istream& rs = opData->requestBody();
        std::string body;
        Poco::StreamCopier::copyToString( rs, body );
        
        std::cout << "=== Create Widget Post Data ===" << std::endl;
        std::cout << body << std::endl;

        // Object was created return info
        opData->responseSetCreated( "w1" );
        opData->responseSetStatusAndReason( HNR_HTTP_CREATED );
    }
    // PUT "/hnode2/test/widgets/{widgetid}"
    else if( "updateWidget" == opID )
    {
        std::string widgetID;

        // Make sure zoneid was provided
        if( opData->getParam( "widgetid", widgetID ) == true )
        {
            // widgetid parameter is required
            opData->responseSetStatusAndReason( HNR_HTTP_BAD_REQUEST );
            opData->responseSend();
            return; 
        }
        
        std::istream& rs = opData->requestBody();
        std::string body;
        Poco::StreamCopier::copyToString( rs, body );
        
        std::cout << "=== Update Widget Put Data (id: " << widgetID << ") ===" << std::endl;
        std::cout << body << std::endl;

        // Request was successful
        opData->responseSetStatusAndReason( HNR_HTTP_OK );
    }    
    // DELETE "/hnode2/test/widgets/{widgetid}"
    else if( "deleteWidget" == opID )
    {
        std::string widgetID;

        // Make sure zoneid was provided
        if( opData->getParam( "widgetid", widgetID ) == true )
        {
            // widgetid parameter is required
            opData->responseSetStatusAndReason( HNR_HTTP_BAD_REQUEST );
            opData->responseSend();
            return; 
        }

        std::cout << "=== Delete Widget Request (id: " << widgetID << ") ===" << std::endl;

        // Request was successful
        opData->responseSetStatusAndReason( HNR_HTTP_OK );
    }
    // PUT "/hnode2/test/health"
    else if( "putTestHealth" == opID )
    {
        // Get access to payload
        std::istream& rs = opData->requestBody();

        // Parse the json body of the request
        try
        {
            std::string component = HNDH_ROOT_COMPID;
            std::string status = "OK";
            uint errCode = 200;

            // Attempt to parse the json
            pjs::Parser parser;
            pdy::Var varRoot = parser.parse( rs );

            // Get a pointer to the root object
            pjs::Object::Ptr jsRoot = varRoot.extract< pjs::Object::Ptr >();

            if( jsRoot->has( "component" ) )
                component = jsRoot->getValue<std::string>( "component" );

            if( jsRoot->has( "status" ) )
                status = jsRoot->getValue<std::string>( "status" );

            if( jsRoot->has( "errCode" ) )
                errCode = jsRoot->getValue<uint>( "errCode" );

            m_hnodeDev.getHealthRef().startUpdateCycle( time(NULL) );

            if( status == "OK" )
            {
                m_hnodeDev.getHealthRef().setComponentStatus( component, HNDH_CSTAT_OK );
                m_hnodeDev.getHealthRef().clearComponentErrMsg( component );
                m_hnodeDev.getHealthRef().clearComponentNote( component );
            }
            else if( status == "UNKNOWN" )
            {
                m_hnodeDev.getHealthRef().setComponentStatus( component, HNDH_CSTAT_UNKNOWN );
                m_hnodeDev.getHealthRef().clearComponentErrMsg( component );
            }
            else if( status == "FAILED" )
            {
                m_hnodeDev.getHealthRef().setComponentStatus( component, HNDH_CSTAT_FAILED );
                m_hnodeDev.getHealthRef().setComponentErrMsg( component, errCode, m_errStrCode, errCode );
            }
            else if( status == "NOTE" )
            {
                m_hnodeDev.getHealthRef().setComponentNote( component, m_noteStrCode );
            }

            m_hnodeDev.getHealthRef().completeUpdateCycle();

        }
        catch( Poco::Exception ex )
        {
            std::cout << "putTestHealth exception: " << ex.displayText() << std::endl;
            opData->responseSetStatusAndReason( HNR_HTTP_INTERNAL_SERVER_ERROR );
            opData->responseSend();
            return;
        }

        // Request was successful
        opData->responseSetStatusAndReason( HNR_HTTP_OK );
    }        
    else
    {
        // Send back not implemented
        opData->responseSetStatusAndReason( HNR_HTTP_NOT_IMPLEMENTED );
        opData->responseSend();
        return;
    }

    // Return to caller
    opData->responseSend();
}

const std::string g_HNode2TestRest = R"(
{
  "openapi": "3.0.0",
  "info": {
    "description": "",
    "version": "1.0.0",
    "title": ""
  },
  "paths": {
      "/hnode2/test/status": {
        "get": {
          "summary": "Get test device status.",
          "operationId": "getStatus",
          "responses": {
            "200": {
              "description": "successful operation",
              "content": {
                "application/json": {
                  "schema": {
                    "type": "array"
                  }
                }
              }
            },
            "400": {
              "description": "Invalid status value"
            }
          }
        }
      },

      "/hnode2/test/widgets": {
        "get": {
          "summary": "Return made up widget list.",
          "operationId": "getWidgetList",
          "responses": {
            "200": {
              "description": "successful operation",
              "content": {
                "application/json": {
                  "schema": {
                    "type": "object"
                  }
                }
              }
            },
            "400": {
              "description": "Invalid status value"
            }
          }
        },

        "post": {
          "summary": "Create a new widget - dummy.",
          "operationId": "createWidget",
          "responses": {
            "200": {
              "description": "successful operation",
              "content": {
                "application/json": {
                  "schema": {
                    "type": "object"
                  }
                }
              }
            },
            "400": {
              "description": "Invalid status value"
            }
          }
        }
      },

      "/hnode2/test/widgets/{widgetid}": {
        "get": {
          "summary": "Get information about a specific widget - dummy.",
          "operationId": "getWidgetInfo",
          "responses": {
            "200": {
              "description": "successful operation",
              "content": {
                "application/json": {
                  "schema": {
                    "type": "object"
                  }
                }
              }
            },
            "400": {
              "description": "Invalid status value"
            }
          }
        },
        "put": {
          "summary": "Update a specific widget - dummy.",
          "operationId": "updateWidget",
          "responses": {
            "200": {
              "description": "successful operation",
              "content": {
                "application/json": {
                  "schema": {
                    "type": "object"
                  }
                }
              }
            },
            "400": {
              "description": "Invalid status value"
            }
          }
        },
        "delete": {
          "summary": "Delete a specific widget - dummy",
          "operationId": "deleteWidget",
          "responses": {
            "200": {
              "description": "successful operation",
              "content": {
                "application/json": {
                  "schema": {
                    "type": "object"
                  }
                }
              }
            },
            "400": {
              "description": "Invalid status value"
            }
          }
        }
      },

      "/hnode2/test/health": {
        "put": {
          "summary": "Cause a health state transistion",
          "operationId": "putTestHealth",
          "responses": {
            "200": {
              "description": "successful operation",
              "content": {
                "application/json": {
                  "schema": {
                    "type": "array"
                  }
                }
              }
            },
            "400": {
              "description": "Invalid status value"
            }
          }
        }
      }
    }
}
)";

