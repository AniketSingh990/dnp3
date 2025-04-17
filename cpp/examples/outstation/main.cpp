#include <asiodnp3/DNP3Manager.h>
#include <asiodnp3/PrintingSOEHandler.h>
#include <asiodnp3/PrintingChannelListener.h>
#include <asiodnp3/ConsoleLogger.h>
#include <asiodnp3/UpdateBuilder.h>
#include <asiopal/UTCTimeSource.h>
#include <opendnp3/outstation/SimpleCommandHandler.h>
#include <opendnp3/outstation/IUpdateHandler.h>
#include <opendnp3/LogLevels.h>

#include <iostream>
##include <string>

using namespace std;
using namespace opendnp3;
using namespace openpal;
using namespace asiopal;
using namespace asiodnp3;

void ConfigureDatabase(DatabaseConfig& config)
{
    config.analog[0].clazz = PointClass::Class2;
    config.analog[0].svariation = StaticAnalogVariation::Group30Var5;
    config.analog[0].evariation = EventAnalogVariation::Group32Var7;
}

struct State
{
    uint32_t count = 0;
    double value = 0.0;
    bool binary = false;
    DoubleBit dbit = DoubleBit::DETERMINED_OFF;
};

void AddUpdates(UpdateBuilder& builder, State& state, const std::string& input)
{
    cout << "\nBefore Updates:\n";
    cout << "Counter: " << state.count << ", Analog: " << state.value
         << ", Binary: " << (state.binary ? "True" : "False")
         << ", DoubleBit: " << (state.dbit == DoubleBit::DETERMINED_ON ? "ON" : "OFF") << endl;

    for (char c : input)
    {
        switch (c)
        {
        case 'c':
            builder.Update(Counter(state.count), 0);
            ++state.count;
            break;
        case 'a':
            builder.Update(Analog(state.value), 0);
            state.value += 1.0;
            break;
        case 'b':
            builder.Update(Binary(state.binary), 0);
            state.binary = !state.binary;
            break;
        case 'd':
            builder.Update(DoubleBitBinary(state.dbit), 0);
            state.dbit = (state.dbit == DoubleBit::DETERMINED_OFF) ? DoubleBit::DETERMINED_ON : DoubleBit::DETERMINED_OFF;
            break;
        default:
            cerr << "Unknown input character: " << c << endl;
            break;
        }
    }

    cout << "After Updates:\n";
    cout << "Counter: " << state.count << ", Analog: " << state.value
         << ", Binary: " << (state.binary ? "True" : "False")
         << ", DoubleBit: " << (state.dbit == DoubleBit::DETERMINED_ON ? "ON" : "OFF") << endl;
}

int main(int argc, char* argv[])
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

    State state;
    std::string input;

    while (true)
    {
        cout << "\nEnter one or more measurement updates:\n";
        cout << "c = counter, b = binary, d = doublebit, a = analog, 'quit' = exit\n> ";
        cin >> input;

        if (input == "quit") break;

        UpdateBuilder builder;
        AddUpdates(builder, state, input);

        outstation->Apply(builder.Build());

        cout << "Current State:\n";
        cout << "Counter: " << state.count << ", Analog: " << state.value
             << ", Binary: " << (state.binary ? "True" : "False")
             << ", DoubleBit: " << (state.dbit == DoubleBit::DETERMINED_ON ? "ON" : "OFF") << endl;
    }

    return 0;
}
