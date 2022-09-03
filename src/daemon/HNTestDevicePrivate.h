#ifndef __HN_TEST_DEVICE_PRIVATE_H__
#define __HN_TEST_DEVICE_PRIVATE_H__

#include <string>
#include <vector>

#include "Poco/Util/ServerApplication.h"
#include "Poco/Util/OptionSet.h"

#include <hnode2/HNodeDevice.h>
#include <hnode2/HNodeConfig.h>
#include <hnode2/HNEPLoop.h>
#include <hnode2/HNReqWaitQueue.h>

#define HNODE_TEST_DEVTYPE   "hnode2-test-device"

typedef enum HNTestDeviceResultEnum
{
  HNTD_RESULT_SUCCESS,
  HNTD_RESULT_FAILURE,
  HNTD_RESULT_BAD_REQUEST,
  HNTD_RESULT_SERVER_ERROR
}HNTD_RESULT_T;

class HNTestDevice : public Poco::Util::ServerApplication, public HNDEPDispatchInf, public HNDEventNotifyInf, public HNEPLoopCallbacks 
{
    private:
        bool _helpRequested   = false;
        bool _debugLogging    = false;
        bool _instancePresent = false;

        std::string _instance; 
        std::string m_instanceName;

        HNodeDevice m_hnodeDev;

        HNEPTrigger m_configUpdateTrigger;

        HNEPLoop m_testDeviceEvLoop;

        // Format string codes
        uint m_errStrCode;
        uint m_noteStrCode;

        // Health component ids
        std::string m_hc1ID;
        std::string m_hc2ID;
        std::string m_hc3ID;

        // Keep track of health state change simulation
        uint m_healthStateSeq;

        void displayHelp();

        bool configExists();
        HNTD_RESULT_T initConfig();
        HNTD_RESULT_T readConfig();
        HNTD_RESULT_T updateConfig();

        void generateNewHealthState();

    protected:
        // HNDevice REST callback
        virtual void dispatchEP( HNodeDevice *parent, HNOperationData *opData );

        // Notification for hnode device config changes.
        virtual void hndnConfigChange( HNodeDevice *parent );

        // Event loop functions
        virtual void loopIteration();
        virtual void timeoutEvent();
        virtual void fdEvent( int sfd );
        virtual void fdError( int sfd );

        // Poco funcions
        void defineOptions( Poco::Util::OptionSet& options );
        void handleOption( const std::string& name, const std::string& value );
        int main( const std::vector<std::string>& args );


};

#endif // __HN_TEST_DEVICE_PRIVATE_H__
