#include "pump_database.hpp"

#include <iostream>

// 自定义GSL错误处理器
void gsl_error_handler(const char *reason, const char *file, int line, int gsl_errno)
{
    throw std::runtime_error(std::string("GSL Error: ") + reason);
}

PumpDatabase::PumpDatabase()
{
    gsl_set_error_handler(gsl_error_handler);
}

PumpDatabase::PumpDatabase(const std::string &file_name) : file_name_(file_name)
{
    loadFromFile(file_name_);
    gsl_set_error_handler(gsl_error_handler);
}

// 文件加载函数
void PumpDatabase::loadFromFile(const std::string &file_name)
{
    try
    {
        std::ifstream ifs(file_name);
        if (!ifs.is_open())
        {
            throw std::runtime_error("Failed to open file: " + file_name);
        }
        json j;
        ifs >> j;
        loadFromJson(j);
        ifs.close();
    }
    catch (const std::exception &e)
    {
        std::cerr << "Error loading file: " << e.what() << std::endl;
        pumps_.clear();
    }
    file_name_ = file_name;
}

void PumpDatabase::saveToFile()
{
    saveToFile(file_name_);
}

void PumpDatabase::saveToFile(const std::string &file_name)
{
    try
    {
        json j;
        saveToJson(j);

        // 备份旧文件
        if (!file_name.empty())
        {
            time_t now = std::time(nullptr);
            std::stringstream ss;
            ss << file_name << "."
               << std::put_time(std::localtime(&now), "%Y%m%d%H%M%S");
            std::rename(file_name.c_str(), ss.str().c_str());
        }

        std::ofstream ofs(file_name);
        ofs << std::setw(4) << j;
    }
    catch (const std::exception &e)
    {
        std::cerr << "Save failed: " << e.what() << std::endl;
    }
}

// JSON序列化函数
void PumpDatabase::loadFromJson(const json &j)
{
    pumps_.clear();
    for (const auto &it : j.items())
    {
        const std::string &pump_name = it.key();
        const json &pump_data = it.value();

        PumpData pd;
        pd.pump_name = pump_name;
        pd.target_flow_rate_offset = pump_data.value("target_flow_rate_offset", 0.0);

        // 加载转速-流量数据
        for (const auto &point : pump_data["rpm_flow_points"])
        {
            pd.rpm_flow_points.emplace_back(point["rpm"].get<int>(),
                                            point["flow_rate"].get<double>());
        }

        // 加载校准数据
        for (const auto &point : pump_data["rpm_flow_calibrated"])
        {
            pd.rpm_flow_calibrated.emplace_back(
                point["rpm"].get<double>(),
                point["flow_rate"].get<double>());
        }

        pumps_.push_back(pd);
    }
}

void PumpDatabase::saveToJson(json &j) const
{
    for (const auto &pd : pumps_)
    {
        json &pump_json = j[pd.pump_name];
        pump_json["target_flow_rate_offset"] = pd.target_flow_rate_offset;

        // 转速-流量原始数据
        json rpm_flow_array = json::array();
        for (const auto &p : pd.rpm_flow_points)
        {
            rpm_flow_array.push_back({{"rpm", p.rpm},
                                      {"flow_rate", p.flow_rate}});
        }
        pump_json["rpm_flow_points"] = rpm_flow_array;

        // 校准后数据
        json cal_array = json::array();
        for (const auto &p : pd.rpm_flow_calibrated)
        {
            cal_array.push_back({{"rpm", p.rpm},
                                 {"flow_rate", p.flow_rate}});
        }
        pump_json["rpm_flow_calibrated"] = cal_array;
    }
}

// 核心计算函数
double PumpDatabase::calculateFlowRate(const std::string &pump_name, double rpm)
{
    PumpData *pd = findPump(pump_name);
    if (!pd)
        return -1.0;

    const auto &points = pd->rpm_flow_points;
    size_t n = points.size();

    if (n < 1)
        return -1.0;

    try
    {
        std::vector<double> coeff;
        double chisq;
        polyfit(points, 2, coeff, chisq); // 2阶多项式拟合

        double result = 0.0;
        for (size_t i = 0; i < coeff.size(); ++i)
        {
            result += coeff[i] * std::pow(rpm, i);
        }
        return result;
    }
    catch (const std::exception &e)
    {
        // 降级到线性回归
        double sum_x = 0, sum_y = 0, sum_xx = 0, sum_xy = 0;
        for (const auto &p : points)
        {
            sum_x += p.rpm;
            sum_y += p.flow_rate;
            sum_xx += p.rpm * p.rpm;
            sum_xy += p.rpm * p.flow_rate;
        }

        double m = (n * sum_xy - sum_x * sum_y) / (n * sum_xx - sum_x * sum_x);
        double b = (sum_y - m * sum_x) / n;

        return m * rpm + b;
    }
}

double PumpDatabase::calculateRPM(const std::string &pump_name, double target_flow_rate)
{
    PumpData *pd = findPump(pump_name);
    if (!pd)
        return -1.0;

    const auto &points = pd->rpm_flow_points;
    size_t n = points.size();

    if (n < 1)
        return -1.0;

    try
    {
        // 使用多项式拟合反向计算
        std::vector<double> coeff;
        double chisq;
        polyfit(points, 1, coeff, chisq); // 2阶多项式拟合

        // 求解多项式方程 coeff[0] + coeff[1]*rpm + coeff[2]*rpm^2 = target_flow_rate
        double a = coeff.size() > 2 ? coeff[2] : 0.0;
        double b = coeff.size() > 1 ? coeff[1] : 0.0;
        double c = coeff[0] - target_flow_rate;

        if (a == 0.0)
        {
            // 如果是线性方程
            if (b == 0.0)
                throw std::runtime_error("无法计算RPM：无解");
            return -c / b;
        }

        // 求解二次方程 ax^2 + bx + c = 0
        double discriminant = b * b - 4 * a * c;
        if (discriminant < 0)
            throw std::runtime_error("无法计算RPM：无实数解");

        double sqrt_discriminant = std::sqrt(discriminant);
        double rpm1 = (-b + sqrt_discriminant) / (2 * a);
        double rpm2 = (-b - sqrt_discriminant) / (2 * a);

        // 返回正值解
        if (rpm1 >= 0 && rpm2 >= 0)
            return std::min(rpm1, rpm2);
        else if (rpm1 >= 0)
            return rpm1;
        else if (rpm2 >= 0)
            return rpm2;
        else
            throw std::runtime_error("无法计算RPM：无正解");
    }
    catch (const std::exception &e)
    {
        // 降级到线性插值
        for (size_t i = 1; i < n; ++i)
        {
            if (points[i - 1].flow_rate <= target_flow_rate && target_flow_rate <= points[i].flow_rate)
            {
                double x1 = points[i - 1].rpm;
                double y1 = points[i - 1].flow_rate;
                double x2 = points[i].rpm;
                double y2 = points[i].flow_rate;

                // 线性插值公式
                return x1 + (target_flow_rate - y1) * (x2 - x1) / (y2 - y1);
            }
        }
        return -1.0; // 如果目标流量超出范围
    }
}

// 多项式拟合实现
void PumpDatabase::polyfit(
    const std::vector<FlowRPMPoint> &points,
    int degree,
    std::vector<double> &coefficients,
    double &chisq)
{
    size_t n = points.size();
    if (n <= 0)
        throw std::invalid_argument("No data points.");

    // 初始化GSL结构
    gsl_multifit_linear_workspace *workspace = gsl_multifit_linear_alloc(n, degree + 1);
    gsl_matrix *X = gsl_matrix_alloc(n, degree + 1);
    gsl_vector *Y = gsl_vector_alloc(n);

    for (size_t i = 0; i < n; i++)
    {
        double x = points[i].rpm;
        gsl_matrix_set(X, i, 0, 1.0); // 常数项
        for (int j = 1; j <= degree; j++)
        {
            gsl_matrix_set(X, i, j, std::pow(x, j));
        }
        gsl_vector_set(Y, i, points[i].flow_rate);
    }

    // 执行拟合
    gsl_vector *c = gsl_vector_alloc(degree + 1);
    gsl_matrix *cov = gsl_matrix_alloc(degree + 1, degree + 1);
    int status = gsl_multifit_linear(X, Y, c, cov, &chisq, workspace);
    if (status != GSL_SUCCESS)
        throw std::runtime_error("GSL fitting failed.");

    // 提取系数
    coefficients.resize(degree + 1);
    for (size_t i = 0; i < coefficients.size(); i++)
    {
        coefficients[i] = gsl_vector_get(c, i);
    }

    // 清理资源
    gsl_multifit_linear_free(workspace);
    gsl_matrix_free(X);
    gsl_vector_free(Y);
    gsl_vector_free(c);
    gsl_matrix_free(cov);
}

// 校准功能
void PumpDatabase::calibrateFlowRate(
    const std::string &pump_name,
    const std::vector<FlowRPMPoint> &calibration_data,
    std::string calibration_type)
{
    PumpData *pd = findPump(pump_name);
    if (!pd)
        return;

    if (calibration_type == "LINEAR")
    {
        // 线性校准
        double sum_x = 0, sum_y = 0, sum_xx = 0, sum_xy = 0;
        size_t n = calibration_data.size();

        for (const auto &point : calibration_data)
        {
            double error = point.flow_rate - calculateFlowRate(pump_name, point.rpm);
            sum_x += point.rpm;
            sum_y += error;
            sum_xx += point.rpm * point.rpm;
            sum_xy += point.rpm * error;
        }

        double m = (n * sum_xy - sum_x * sum_y) / (n * sum_xx - sum_x * sum_x);
        double b = (sum_y - m * sum_x) / n;

        pd->rpm_flow_calibrated.clear();
        for (const auto &p : pd->rpm_flow_points)
        {
            pd->rpm_flow_calibrated.push_back({p.rpm,
                                               p.flow_rate + m * p.rpm + b});
        }
    }
    else if (calibration_type == "OFFSET")
    {
        // 偏移量校准
        double total_offset = 0.0;
        for (const auto &point : calibration_data)
        {
            total_offset += (point.flow_rate - calculateFlowRate(pump_name, point.rpm));
        }
        double avg_offset = total_offset / calibration_data.size();

        pd->rpm_flow_calibrated.clear();
        for (const auto &p : pd->rpm_flow_points)
        {
            pd->rpm_flow_calibrated.push_back({p.rpm,
                                               p.flow_rate + avg_offset});
        }
    }
}

// 数据管理函数
bool PumpDatabase::addPump(const PumpData &pump)
{
    if (findPump(pump.pump_name))
        return false; // 名称重复
    pumps_.push_back(pump);
    return true;
}

bool PumpDatabase::removePump(const std::string &pump_name)
{
    auto it = std::remove_if(pumps_.begin(), pumps_.end(),
                             [&](const PumpData &pd)
                             { return pd.pump_name == pump_name; });
    if (it == pumps_.end())
        return false;
    pumps_.erase(it, pumps_.end());
    return true;
}

PumpData *PumpDatabase::findPump(const std::string &pump_name)
{
    for (auto &pd : pumps_)
    {
        if (pd.pump_name == pump_name)
            return &pd;
    }
    return nullptr;
}

bool PumpDatabase::empty()
{
    return pumps_.empty();
}

const PumpData *PumpDatabase::getPump(const std::string &pump_name)
{
    for (const auto &pd : pumps_)
    {
        if (pd.pump_name == pump_name)
            return &pd;
    }
    return nullptr;
}

const std::vector<PumpData> *PumpDatabase::getPumps()
{
    return &pumps_;
}
