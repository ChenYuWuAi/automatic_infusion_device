#include "pump_database.hpp"
#include <iostream>
#include <string>
#include <sstream>
#include <vector>
#include <limits>

using namespace std;

PumpDatabase pump_db;

void clearInput()
{
    std::cin.clear();
    std::cin.ignore(std::numeric_limits<std::streamsize>::max(), '\n');
}

vector<FlowRPMPoint> getRPMFlowPointsFromCin()
{
    vector<FlowRPMPoint> rpm_flow_points;
    clearInput();
    while (true)
    {
        stringstream ss;
        FlowRPMPoint point;
        string cin_buffer;
        getline(cin, cin_buffer);
        // EOF
        if (std::cin.eof())
        {
            return rpm_flow_points;
        }

        if (cin_buffer == "q")
        {
            break;
        }
        else
        {
            // 用空格分割字符串
            ss << cin_buffer;
            ss >> point.rpm >> point.flow_rate;
            cout << "RPM: " << point.rpm << " Flow Rate: " << point.flow_rate << endl;
            rpm_flow_points.push_back(point);
            ss.clear();
        }
    }

    return rpm_flow_points;
}

void addPump()
{
    PumpData new_pump;
    cout << "Please input pump name: ";
    cin >> new_pump.pump_name;

    // EOF
    if (std::cin.eof())
    {
        return;
    }

    cout << "Please input pump RPM-Flow Rate pairs. First RPM next Flow Rate. Enter 'q' to finish.\n";
    new_pump.rpm_flow_points = getRPMFlowPointsFromCin();
    // EOF
    if (std::cin.eof())
    {
        return;
    }

    if (pump_db.addPump(new_pump))
    {
        pump_db.saveToFile();
        cout << "Pump data saved." << endl;
    }
    else
    {
        cout << "Pump with the same name already exists." << endl;
    }
}

void deletePump(string pump_name)
{
    if (pump_db.removePump(pump_name))
    {
        pump_db.saveToFile();
        cout << "Pump deleted." << endl;
    }
    else
    {
        cout << "Pump not found." << endl;
    }
}

void calculateFlowRate(string pump_name, double rpm)
{
    double flow_rate = pump_db.calculateFlowRate(pump_name, rpm);
    if (flow_rate >= 0)
    {
        cout << "Calculated Flow Rate: " << flow_rate << endl;
    }
    else
    {
        cout << "Failed to calculate flow rate. Check pump data." << endl;
    }
}

void calculateRPM(string pump_name, double target_flow_rate)
{
    double rpm = pump_db.calculateRPM(pump_name, target_flow_rate);
    if (rpm >= 0)
    {
        cout << "Calculated RPM: " << rpm << endl;
    }
    else
    {
        cout << "Failed to calculate RPM. Check pump data." << endl;
    }
}

void dumpPumpData(string pump_name)
{
    const PumpData *pump_data = pump_db.getPump(pump_name);
    if (pump_data)
    {
        cout << "Pump name: " << pump_data->pump_name << endl;
        cout << "Target Flow Rate Offset: " << pump_data->target_flow_rate_offset << endl;
        cout << "RPM-Flow Rate points:\n";
        for (const auto &point : pump_data->rpm_flow_points)
        {
            cout << point.rpm << " " << point.flow_rate << endl;
        }
        cout << "Calibrated RPM-Flow Rate points:\n";
        for (const auto &point : pump_data->rpm_flow_calibrated)
        {
            cout << point.rpm << " " << point.flow_rate << endl;
        }
    }
    else
    {
        cout << "Pump not found." << endl;
    }
}

void pumpMenu(string pump_name)
{
    while (true)
    {
        cout << "\nPump Menu for " << pump_name << ":\n";
        cout << "1. Delete pump\n";
        cout << "2. Add RPM-Flow Rate point\n";
        cout << "3. Calculate flow rate\n";
        cout << "4. Calculate RPM\n";
        cout << "5. Dump pump data\n";
        cout << "6. Exit\n";
        cout << "Enter your choice: ";
        int choice;
        cin >> choice;

        if (cin.fail())
        {
            // EOF
            if (cin.eof())
            {
                return;
            }

            clearInput();
            cout << "Invalid input. Please enter a number.\n";
            continue;
        }

        switch (choice)
        {
        case 1:
            deletePump(pump_name);
            return;
        case 2:
        {
            cout << "Please input RPM-Flow Rate pairs. First RPM next Flow Rate. Enter 'q' to finish.\n";
            vector<FlowRPMPoint> rpm_flow_points = getRPMFlowPointsFromCin();
            PumpData *pump = pump_db.findPump(pump_name);
            if (pump)
            {
                pump->rpm_flow_points.insert(pump->rpm_flow_points.end(), rpm_flow_points.begin(), rpm_flow_points.end());
                pump_db.saveToFile();
                cout << "Pump data updated." << endl;
            }
            else
            {
                cout << "Pump not found." << endl;
            }
            break;
        }
        case 3:
        {
            cout << "Please input RPM: ";
            double rpm;
            cin >> rpm;
            calculateFlowRate(pump_name, rpm);
            break;
        }
        case 4:
        {
            cout << "Please input target flow rate: ";
            double target_flow_rate;
            cin >> target_flow_rate;
            calculateRPM(pump_name, target_flow_rate);
            break;
        }
        case 5:
            dumpPumpData(pump_name);
            break;
        case 6:
            return;
        default:
            cout << "Invalid choice. Please try again.\n";
        }
    }
}

void mainMenu()
{
    while (true)
    {
        cout << "\nMain Menu:\n";
        cout << "1. Select pump\n";
        cout << "2. Add new pump\n";
        cout << "3. Exit\n";
        cout << "Enter your choice: ";
        int choice;
        cin >> choice;

        if (cin.fail())
        {
            // EOF
            if (cin.eof())
            {
                return;
            }
            clearInput();
            cout << "Invalid input. Please enter a number.\n";
            continue;
        }

        switch (choice)
        {
        case 1:
            if (pump_db.empty())
            {
                cout << "No pumps available. Please add a new pump first.\n";
            }
            else
            {
                cout << "Available pumps:\n";
                for (const auto &pump : *pump_db.getPumps())
                {
                    cout << pump.pump_name << endl;
                }

                cout << "Select pump name: ";
                string pump_choice;
                cin >> pump_choice;
                if (pump_db.findPump(pump_choice))
                {
                    pumpMenu(pump_choice);
                }
                else
                {
                    cout << "Pump not found. Please try again.\n";
                }
            }
            break;
        case 2:
            addPump();
            break;
        case 3:
            cout << "Exiting...\n";
            return;
        default:
            cout << "Invalid choice. Please try again.\n";
        }
    }
}

int main()
{
    // 加载泵数据库
    pump_db.loadFromFile("pump_data.json");
    // 主菜单
    mainMenu();
    return 0;
}