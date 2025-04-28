#include <string>
#include <vector>
#include <functional>
#include <nlohmann/json.hpp>
#include <gsl/gsl_multifit.h> // 需要GSL库支持

#include <ctime>
#include <fstream>
#include <sstream>

using json = nlohmann::json;

// 数据结构
struct FlowRPMPoint
{
    double rpm;
    double flow_rate;
    FlowRPMPoint(double r, double f) : rpm(r), flow_rate(f) {}
    FlowRPMPoint() : rpm(0.0), flow_rate(0.0) {}
};

struct PumpData
{
    std::string pump_name;
    double target_flow_rate_offset = 0.0;          // 目标流量偏移量
    std::vector<FlowRPMPoint> rpm_flow_points;     // 转速-流量原始数据
    std::vector<FlowRPMPoint> rpm_flow_calibrated; // 校准后的转速-流量数据
};

class PumpDatabase
{
public:
    PumpDatabase();
    explicit PumpDatabase(const std::string &file_name);
    ~PumpDatabase() = default;

    // 文件操作
    void loadFromFile(const std::string &file_name);
    void saveToFile();
    void saveToFile(const std::string &file_name);

    // 核心计算
    double calculateFlowRate(const std::string &pump_name, double rpm);
    double calculateRPM(const std::string &pump_name, double flow_rate);

    // 校准功能
    void calibrateFlowRate(const std::string &pump_name,
                           const std::vector<FlowRPMPoint> &calibration_data,
                           std::string calibration_type);

    // 数据管理
    bool addPump(const PumpData &pump);
    bool removePump(const std::string &pump_name);
    bool updatePump(const PumpData &updated_pump);
    bool empty();

    // 获取数据
    const PumpData *getPump(const std::string &pump_name);
    const std::vector<PumpData> *getPumps();

    PumpData *findPump(const std::string &pump_name);

private:
    // 私有辅助函数
    void loadFromJson(const json &j);
    void saveToJson(json &j) const;

    static void polyfit(const std::vector<FlowRPMPoint> &points,
                        int degree,
                        std::vector<double> &coefficients,
                        double &chisq);

    std::vector<PumpData> pumps_;
    std::string file_name_;
};