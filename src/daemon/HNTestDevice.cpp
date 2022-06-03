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
#include <hnode2/HNSwitchDaemon.h>

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

    std::cout << "Looking for config file" << std::endl;
    
    if( configExists() == false )
    {
        initConfig();
    }

    readConfig();

    // Start up the hnode device
    m_hnodeDev.start();

    waitForTerminationRequest();

    return Application::EXIT_OK;
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
HNTestDevice::dispatchEP( HNodeDevice *parent, HNOperationData *opData )
{
    //HNIDActionRequest action;

    std::cout << "HNTestDevice::dispatchEP() - entry" << std::endl;
    std::cout << "  dispatchID: " << opData->getDispatchID() << std::endl;
    std::cout << "  opID: " << opData->getOpID() << std::endl;
    std::cout << "  thread: " << std::this_thread::get_id() << std::endl;

    std::string opID = opData->getOpID();
#if 0
    // GET "/hnode2/irrigation/status"
    if( "getStatus" == opID )
    {
        action.setType( HNID_AR_TYPE_IRRSTATUS );
    }
    // GET "/hnode2/irrigation/switches"
    else if( "getSwitchList" == opID )
    {
        action.setType( HNID_AR_TYPE_SWLIST );
    }
    // GET "/hnode2/irrigation/zones"
    else if( "getZoneList" == opID )
    {
        action.setType( HNID_AR_TYPE_ZONELIST );
    }
    // GET "/hnode2/irrigation/zones/{zoneid}"
    else if( "getZoneInfo" == opID )
    {
        std::string zoneID;

        if( opData->getParam( "zoneid", zoneID ) == true )
        {
            opData->responseSetStatusAndReason( HNR_HTTP_INTERNAL_SERVER_ERROR );
            opData->responseSend();
            return; 
        }

        action.setType( HNID_AR_TYPE_ZONEINFO );
        action.setZoneID( zoneID );
    }
    // POST "/hnode2/irrigation/zones"
    else if( "createZone" == opID )
    {
        action.setType( HNID_AR_TYPE_ZONECREATE );

        std::istream& bodyStream = opData->requestBody();
        action.decodeZoneUpdate( bodyStream );
    }
    // PUT "/hnode2/irrigation/zones/{zoneid}"
    else if( "updateZone" == opID )
    {
        std::string zoneID;

        // Make sure zoneid was provided
        if( opData->getParam( "zoneid", zoneID ) == true )
        {
            // zoneid parameter is required
            opData->responseSetStatusAndReason( HNR_HTTP_BAD_REQUEST );
            opData->responseSend();
            return; 
        }
        action.setType( HNID_AR_TYPE_ZONEUPDATE );
        action.setZoneID( zoneID );

        std::istream& bodyStream = opData->requestBody();
        action.decodeZoneUpdate( bodyStream );
    }
    // DELETE "/hnode2/irrigation/zones/{zoneid}"
    else if( "deleteZone" == opID )
    {
        std::string zoneID;

        // Make sure zoneid was provided
        if( opData->getParam( "zoneid", zoneID ) == true )
        {
            // zoneid parameter is required
            opData->responseSetStatusAndReason( HNR_HTTP_BAD_REQUEST );
            opData->responseSend();
            return; 
        }

        action.setType( HNID_AR_TYPE_ZONEDELETE );
        action.setZoneID( zoneID );
    }
    else if( "getPlacementList" == opID )
    {
        action.setType( HNID_AR_TYPE_PLACELIST );
    }
    else if( "createPlacement" == opID )
    {
        action.setType( HNID_AR_TYPE_PLACECREATE );

        std::istream& bodyStream = opData->requestBody();
        action.decodePlacementUpdate( bodyStream );
    }
    else if( "getPlacement" == opID )
    {
        std::string placementID;

        if( opData->getParam( "placementid", placementID ) == true )
        {
            opData->responseSetStatusAndReason( HNR_HTTP_INTERNAL_SERVER_ERROR );
            opData->responseSend();
            return; 
        }

        action.setType( HNID_AR_TYPE_PLACEINFO );
        action.setPlacementID( placementID );
    }
    else if( "updatePlacement" == opID )
    {
        std::string placementID;

        // Make sure zoneid was provided
        if( opData->getParam( "placementid", placementID ) == true )
        {
            // zoneid parameter is required
            opData->responseSetStatusAndReason( HNR_HTTP_BAD_REQUEST );
            opData->responseSend();
            return; 
        }

        action.setType( HNID_AR_TYPE_PLACEUPDATE );
        action.setPlacementID( placementID );

        std::istream& bodyStream = opData->requestBody();
        action.decodePlacementUpdate( bodyStream );
    }
    else if( "deletePlacement" == opID )
    {
        std::string placementID;

        // Make sure zoneid was provided
        if( opData->getParam( "placementid", placementID ) == true )
        {
            // eventid parameter is required
            opData->responseSetStatusAndReason( HNR_HTTP_BAD_REQUEST );
            opData->responseSend();
            return; 
        }

        action.setType( HNID_AR_TYPE_PLACEDELETE );
        action.setPlacementID( placementID );
    }
    else if( "getModifiersList" == opID )
    {
        action.setType( HNID_AR_TYPE_MODIFIERSLIST );
    }
    else if( "createModifier" == opID )
    {
        action.setType( HNID_AR_TYPE_MODIFIERCREATE );

        std::istream& bodyStream = opData->requestBody();
        action.decodeModifierUpdate( bodyStream );
    }
    else if( "getModifier" == opID )
    {
        std::string modifierID;

        if( opData->getParam( "modifierid", modifierID ) == true )
        {
            opData->responseSetStatusAndReason( HNR_HTTP_INTERNAL_SERVER_ERROR );
            opData->responseSend();
            return; 
        }

        action.setType( HNID_AR_TYPE_MODIFIERINFO );
        action.setModifierID( modifierID );
    }
    else if( "updateModifier" == opID )
    {
        std::string modifierID;

        // Make sure zoneid was provided
        if( opData->getParam( "modifierid", modifierID ) == true )
        {
            // zoneid parameter is required
            opData->responseSetStatusAndReason( HNR_HTTP_BAD_REQUEST );
            opData->responseSend();
            return; 
        }

        action.setType( HNID_AR_TYPE_MODIFIERUPDATE );
        action.setModifierID( modifierID );

        std::istream& bodyStream = opData->requestBody();
        action.decodeModifierUpdate( bodyStream );
    }
    else if( "deleteModifier" == opID )
    {
        std::string modifierID;

        // Make sure zoneid was provided
        if( opData->getParam( "modifierid", modifierID ) == true )
        {
            // eventid parameter is required
            opData->responseSetStatusAndReason( HNR_HTTP_BAD_REQUEST );
            opData->responseSend();
            return; 
        }

        action.setType( HNID_AR_TYPE_MODIFIERDELETE );
        action.setModifierID( modifierID );
    }
    else if( "getScheduleInfo" == opID )
    {
        action.setType( HNID_AR_TYPE_SCHINFO );
    }
    else if( "getSchedulerState" == opID )
    {
        action.setType( HNID_AR_TYPE_GETSCHSTATE );
    }
    else if( "setSchedulerState" == opID )
    {
        action.setType( HNID_AR_TYPE_SETSCHSTATE );

        std::istream& bodyStream = opData->requestBody();
        action.decodeSchedulerState( bodyStream );
    }
    else if( "putZoneControlRequest" == opID )
    {
        action.setType( HNID_AR_TYPE_ZONECTL );

        std::istream& bodyStream = opData->requestBody();
        action.decodeZoneCtrl( bodyStream );
    }
    else
    {
        // Send back not implemented
        opData->responseSetStatusAndReason( HNR_HTTP_NOT_IMPLEMENTED );
        opData->responseSend();
        return;
    }

    std::cout << "Start Action - client: " << action.getType() << "  thread: " << std::this_thread::get_id() << std::endl;

    // Submit the action and block for response
    m_actionQueue.postAndWait( &action );

    std::cout << "Finish Action - client" << "  thread: " << std::this_thread::get_id() << std::endl;

    // Determine what happened
    switch( action.getStatus() )
    {
        case HNRW_RESULT_SUCCESS:
        {
            std::string cType;
            std::string objID;


            // See if response content should be generated
            if( action.hasRspContent( cType ) )
            {
                // Set response content type
                opData->responseSetChunkedTransferEncoding( true );
                opData->responseSetContentType( cType );

                // Render any response content
                std::ostream& ostr = opData->responseSend();
            
                if( action.generateRspContent( ostr ) == true )
                {
                    opData->responseSetStatusAndReason( HNR_HTTP_INTERNAL_SERVER_ERROR );
                    opData->responseSend();
                    return;
                }
            }

            // Check if a new object was created.
            if( action.hasNewObject( objID ) )
            {
                // Object was created return info
                opData->responseSetCreated( objID );
                opData->responseSetStatusAndReason( HNR_HTTP_CREATED );
            }
            else
            {
                // Request was successful
                opData->responseSetStatusAndReason( HNR_HTTP_OK );
            }
        }
        break;

        case HNRW_RESULT_FAILURE:
            opData->responseSetStatusAndReason( HNR_HTTP_INTERNAL_SERVER_ERROR );
        break;

        case HNRW_RESULT_TIMEOUT:
#endif        
            opData->responseSetStatusAndReason( HNR_HTTP_INTERNAL_SERVER_ERROR );
//        break;
#if 0
    }
#endif

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
      "/hnode2/irrigation/status": {
        "get": {
          "summary": "Get irrigation device status.",
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

      "/hnode2/irrigation/switches": {
        "get": {
          "summary": "Get a list of controllable switches.",
          "operationId": "getSwitchList",
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

      "/hnode2/irrigation/zones": {
        "get": {
          "summary": "Get information about controlled zones.",
          "operationId": "getZoneList",
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
          "summary": "Create a new zone association.",
          "operationId": "createZone",
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

      "/hnode2/irrigation/zones/{zoneid}": {
        "get": {
          "summary": "Get information about a specific zone.",
          "operationId": "getZoneInfo",
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
          "summary": "Update existing zone settings.",
          "operationId": "updateZone",
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
          "summary": "Delete an existing zone.",
          "operationId": "deleteZone",
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


      "/hnode2/irrigation/schedule": {
        "get": {
          "summary": "Get information about the current zone schedule.",
          "operationId": "getScheduleInfo",
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

      "/hnode2/irrigation/schedule/state": {
        "get": {
          "summary": "Get information about scheduler state.",
          "operationId": "getSchedulerState",
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
          "summary": "Set scheduler to a specific state.",
          "operationId": "setSchedulerState",
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


      "/hnode2/irrigation/placement": {
        "get": {
          "summary": "Get list of schedule placement.",
          "operationId": "getPlacementList",
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
          "summary": "Create a new schedule placement.",
          "operationId": "createPlacement",
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

      "/hnode2/irrigation/placement/{placementid}": {
        "get": {
          "summary": "Get information about a specific schedule placement.",
          "operationId": "getPlacement",
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
          "summary": "Update an existing placement.",
          "operationId": "updatePlacement",
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
          "summary": "Delete an existing placement.",
          "operationId": "deletePlacement",
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

      "/hnode2/irrigation/modifier": {
        "get": {
          "summary": "Get list of zone modifiers.",
          "operationId": "getModifiersList",
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
          "summary": "Create a new zone modifier.",
          "operationId": "createModifier",
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

      "/hnode2/irrigation/modifier/{modifierid}": {
        "get": {
          "summary": "Get information about a specific zone modifier.",
          "operationId": "getModifier",
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
          "summary": "Update an existing modifier.",
          "operationId": "updateModifier",
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
          "summary": "Delete an existing modifier.",
          "operationId": "deleteModifier",
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

      "/hnode2/irrigation/zonectl": {
        "put": {
          "summary": "Send manual control request for one or more zones.",
          "operationId": "putZoneControlRequest",
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
      }
    }
}
)";

