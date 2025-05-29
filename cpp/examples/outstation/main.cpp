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

#include <iostream>
#include <string>
#include <vector>
#include <sstream>

using namespace std;
using namespace opendnp3;
using namespace openpal;
using namespace asiopal;
using namespace asiodnp3;
using boost::asio::ip::tcp;

// Helper function to split string by delimiter
vector<string> Split(const string& s, char delimiter)
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

void ConfigureDatabase(DatabaseConfig& config)
{
    // Adjust sizes if needed, here 3 analog points (temp, pressure, humidity), 1 binary
    config.analog.resize(3);
    config.binary.resize(1);

    for (auto& a : config.analog)
    {
        a.clazz = PointClass::Class2;
        a.svariation = StaticAnalogVariation::Group30Var5;
        a.evariation = EventAnalogVariation::Group32Var7;
    }

    config.binary[0].clazz = PointClass::Class1;
}

int main()
{
    const uint32_t FILTERS = levels::NORMAL | levels::ALL_COMMS;

    DNP3Manager manager(1, ConsoleLogger::Create());

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
    config.link.KeepAliveTimeout = openpal::TimeDuration::Max();

    ConfigureDatabase(config.dbConfig);

    auto outstation = channel->AddOutstation(
        "outstation",
        SuccessCommandHandler::Create(),
        DefaultOutstationApplication::Create(),
        config
    );

    outstation->Enable();

    cout << "DNP3 Outstation running..." << endl;
    cout << "Listening for TCP sensor data on port 15000..." << endl;

    // Boost.Asio setup for TCP socket on port 15000
    boost::asio::io_context io_context;
    tcp::acceptor acceptor(io_context, tcp::endpoint(tcp::v4(), 15000));

    while (true)
    {
        try
        {
            tcp::socket socket(io_context);
            acceptor.accept(socket);

            boost::asio::streambuf buffer;
            boost::system::error_code error;

            // Read until newline or end of data
            boost::asio::read_until(socket, buffer, '\n', error);

            if (error && error != boost::asio::error::eof)
            {
                cerr << "Read error: " << error.message() << endl;
                continue;
            }

            istream input_stream(&buffer);
            string line;
            getline(input_stream, line);

            // Strip any trailing carriage return (for Windows clients)
            if (!line.empty() && line.back() == '\r')
                line.pop_back();

            cout << "[DATA RECEIVED] " << line << endl;

            // Parse CSV data
            vector<string> tokens = Split(line, ',');

            if (tokens.size() != 4)
            {
                cerr << "Invalid data format received. Expected 4 comma-separated values." << endl;
                continue;
            }

            try
            {
                double temperature = stod(tokens[0]);
                double pressure = stod(tokens[1]);
                double humidity = stod(tokens[2]);
                bool binary_state = stoi(tokens[3]) != 0;

                UpdateBuilder builder;
                builder.Update(Analog(temperature), 0);  // point 0 = temperature
                builder.Update(Analog(pressure), 1);     // point 1 = pressure
                builder.Update(Analog(humidity), 2);     // point 2 = humidity
                builder.Update(Binary(binary_state), 0); // binary point 0

                outstation->Apply(builder.Build());

                cout << "[INFO] Updated DNP3 Outstation with Temperature=" << temperature
                     << ", Pressure=" << pressure
                     << ", Humidity=" << humidity
                     << ", Binary=" << binary_state << endl;
            }
            catch (const exception& e)
            {
                cerr << "Data parsing error: " << e.what() << endl;
            }
        }
        catch (const exception& e)
        {
            cerr << "Exception: " << e.what() << endl;
        }
    }

    return 0;
}
