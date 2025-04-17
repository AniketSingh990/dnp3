#include <openpal/logging/LogLevels.h>
#include <asiodnp3/DNP3Manager.h>
#include <asiodnp3/DefaultOutstationApplication.h>
#include <asiodnp3/DefaultListenCallbacks.h>
#include <opendnp3/outstation/DatabaseTemplates.h>
#include <opendnp3/outstation/UpdateBuilder.h>

#include <iostream>
#include <thread>
#include <chrono>

using namespace std;
using namespace openpal;
using namespace opendnp3;
using namespace asiodnp3;

int main()
{
    DNP3Manager manager(1);
    auto logger = manager.GetLogger();

    auto channel = manager.AddTCPServer("server", levels::NORMAL, ChannelRetry::Default(), "0.0.0.0", 20000, nullptr);

    OutstationStackConfig config;
    config.outstation.eventBufferConfig = EventBufferConfig::AllTypes(10);
    config.dbConfig.analog[0].clazz = PointClass::Class1;

    auto outstation = channel->AddOutstation("outstation", [](DatabaseConfig& db) {
        db.analog[0].clazz = PointClass::Class1;
    }, DefaultOutstationApplication::Create(), config);

    outstation->Enable();

    cout << "RTU running and serving voltage data on port 20000..." << endl;

    while (true)
    {
        Analog voltage(230.0); // voltage in volts
        outstation->Update(voltage, 0); // update index 0
        this_thread::sleep_for(chrono::seconds(5));
    }

    return 0;
}
