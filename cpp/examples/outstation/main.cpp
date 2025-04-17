#include <asiodnp3/DNP3Manager.h>
#include <asiodnp3/PrintingSOEHandler.h>
#include <asiodnp3/PrintingChannelListener.h>
#include <asiodnp3/ConsoleLogger.h>
#include <asiodnp3/UpdateBuilder.h>
#include <asiopal/UTCTimeSource.h>
#include <opendnp3/outstation/SimpleCommandHandler.h>
#include <opendnp3/outstation/IUpdateHandler.h>
#include <opendnp3/LogLevels.h>
#include <boost/asio.hpp>
#include <string>
#include <thread>
#include <iostream>
#include <sstream>
#include <vector>

using namespace std;
using namespace opendnp3;
using namespace openpal;
using namespace asiopal;
using namespace asiodnp3;
using boost::asio::ip::tcp;

shared_ptr<IOutstation> outstation_global;

void ConfigureDatabase(DatabaseConfig& config)
{
    // Configure digital input points for binary data (0 or 1)
    config.digital[0].clazz = PointClass::Class2;
    config.digital[0].svariation = StaticBinaryVariation::Group1Var2;
    config.digital[1] = config.digital[0]; // Another digital input, you can add more if needed
}

vector<string> split(const string& s, char delimiter)
{
    vector<string> tokens;
    string token;
    istringstream tokenStream(s);
    while (getline(tokenStream, token, delimiter))
    {
        tokens.push_back(token);
    }
    return tokens;
}

void start_tcp_sensor_listener()
{
    try
    {
        boost::asio::io_context io_context;
        // Listen on port 20001 for the binary data
        tcp::acceptor acceptor(io_context, tcp::endpoint(tcp::v4(), 20001));
        cout << "[INFO] Listening for binary sensor data on port 20001..." << endl;
        while (true)
        {
            tcp::socket socket(io_context);
            acceptor.accept(socket);
            char data[1024] = {0};
            size_t length = socket.read_some(boost::asio::buffer(data));
            string received(data, length);
            cout << "[RECEIVED] Binary data: " << received << endl;
            
            if (received == "0" || received == "1")
            {
                UpdateBuilder builder;
                builder.Update(Binary(received == "1"), 0); // Binary data (1 or 0)
                outstation_global->Apply(builder.Build());
                cout << "[UPDATED] Binary data updated in DNP3 outstation." << endl;
            }
            else
            {
                cout << "[ERROR] Invalid binary data received." << endl;
            }
            socket.close();
        }
    }
    catch (exception& e)
    {
        cerr << "[ERROR] TCP Server: " << e.what() << endl;
    }
}

int main(int argc, char* argv[])
{
    const uint32_t FILTERS = levels::NORMAL | levels::ALL_COMMS;
    DNP3Manager manager(1, ConsoleLogger::Create());

    // DNP3 listening on port 20000
    auto channel = manager.AddTCPServer(
        "server",
        FILTERS,
        ChannelRetry::Default(),
        "0.0.0.0",
        20000,
        PrintingChannelListener::Create()
    );

    OutstationStackConfig config(DatabaseSizes::AllTypes(10));
    config.outstation.eventBufferConfig = EventBufferConfig::AllTypes(10);
    config.outstation.params.allowUnsolicited = true;
    config.link.LocalAddr = 10;
    config.link.RemoteAddr = 1;
    config.link.KeepAliveTimeout = TimeDuration::Max();
    ConfigureDatabase(config.dbConfig);

    outstation_global = channel->AddOutstation(
        "outstation",
        SuccessCommandHandler::Create(),
        DefaultOutstationApplication::Create(),
        config
    );

    outstation_global->Enable();
    cout << "[INFO] DNP3 Outstation started. Listening on 20000" << endl;

    // Start TCP sensor server (on 20001)
    thread tcp_thread(start_tcp_sensor_listener);
    tcp_thread.join();

    return 0;
}
