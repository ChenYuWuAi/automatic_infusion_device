#include "infusion_state_machine.hpp"
#include "logger.hpp"
#include <chrono>
#include <string.h>
#include "pn532.h"
#include "pn532_rpi.h"
#include "buzzer_songs.h"
#include "sound_effect_manager.hpp"

// 状态名称定义
const std::string STATE_IDLE = "IDLE";
const std::string STATE_VERIFY_PENDING = "VERIFY_PENDING";
const std::string STATE_VERIFIED = "VERIFIED";
const std::string STATE_PREPARING = "PREPARING";
const std::string STATE_INFUSING = "INFUSING";
const std::string STATE_PAUSED = "PAUSED";
const std::string STATE_EMERGENCY_STOP = "EMERGENCY_STOP";
const std::string STATE_ERROR = "ERROR";

// 动作名称定义
const std::string ACTION_IDLE = "ACTION_IDLE";
const std::string ACTION_VERIFY_PENDING = "ACTION_VERIFY_PENDING";
const std::string ACTION_VERIFIED = "ACTION_VERIFIED";
const std::string ACTION_PREPARING = "ACTION_PREPARING";
const std::string ACTION_INFUSING = "ACTION_INFUSING";
const std::string ACTION_PAUSED = "ACTION_PAUSED";
const std::string ACTION_EMERGENCY_STOP = "ACTION_EMERGENCY_STOP";
const std::string ACTION_ERROR = "ACTION_ERROR";

// 支持多张允许的卡UID
#include <vector>

// 允许的卡UID列表（可根据需要添加更多UID）
static const std::vector<std::vector<uint8_t>> ALLOWED_UIDS = {
    {0xDA, 0xCA, 0xA8, 0x92},
    {0x12, 0x34, 0x56, 0x78},
    {0xAB, 0xCD, 0xEF, 0x01},
    {0x75, 0xa4, 0xe5, 0x03},
};

// 判断UID是否在允许列表中
static bool isUidAllowed(const uint8_t* uid, int uidLen) {
    for (const auto& allowedUid : ALLOWED_UIDS) {
        if (allowedUid.size() == static_cast<size_t>(uidLen) &&
            memcmp(uid, allowedUid.data(), uidLen) == 0) {
            return true;
        }
    }
    return false;
}

// 空闲状态动作
class IdleAction : public openfsm::OpenFSMAction
{
public:
    IdleAction()
    {
        actionName_ = ACTION_IDLE;
    }

    // 进入空闲状态
    void enter(openfsm::OpenFSM &fsm) const override
    {
        auto *context = static_cast<InfusionStateMachine::FSMContext *>(fsm.getCustom());
        if (!context)
            return;

        // 停止电机
        context->motorDriver->setSpeed(0);

        // 更新状态
        context->pumpState->state.store(IDLE);
        context->pumpState->current_flow_rate.store(0.0);
        context->pumpState->current_speed.store(0.0);

        InfusionLogger::info("已进入空闲状态");
    }

    // 空闲状态更新
    void update(openfsm::OpenFSM &fsm) const override
    {
        auto *context = static_cast<InfusionStateMachine::FSMContext *>(fsm.getCustom());
        if (!context)
            return;

        // 检查状态是否被外部更新
        PumpControlState currentState = context->pumpState->state.load();
        if (currentState != IDLE)
        {
            // 状态被外部改变，进行状态转换
            switch (currentState)
            {
            case VERIFY_PENDING:
                fsm.enterState(STATE_VERIFY_PENDING);
                break;
            case PREPARING:
                fsm.enterState(STATE_PREPARING);
                break;
            case EMERGENCY_STOP:
                fsm.enterState(STATE_EMERGENCY_STOP);
                break;
            case ERROR:
                fsm.enterState(STATE_ERROR);
                break;
            default:
                // 其他状态不允许从IDLE直接转换
                InfusionLogger::warn("不允许从IDLE状态直接转换到状态: {}", static_cast<int>(currentState));
                context->pumpState->state.store(IDLE);
                break;
            }
        }
    }

    // 离开空闲状态
    void exit(openfsm::OpenFSM &fsm) const override
    {
        auto *context = static_cast<InfusionStateMachine::FSMContext *>(fsm.getCustom());
        if (!context)
            return;

        InfusionLogger::debug("正在离开空闲状态");
    }
};

// 验证待处理状态动作
class VerifyPendingAction : public openfsm::OpenFSMAction
{
public:
    VerifyPendingAction()
    {
        actionName_ = ACTION_VERIFY_PENDING;
    }

    void enter(openfsm::OpenFSM &fsm) const override
    {
        auto *context = static_cast<InfusionStateMachine::FSMContext *>(fsm.getCustom());
        if (!context)
            return;

        // 确保电机停止
        context->motorDriver->setSpeed(0);

        // 更新状态
        context->pumpState->state.store(VERIFY_PENDING);

        InfusionLogger::info("已进入验证待处理状态，等待验证");

        if (g_soundEffectManager)
            g_soundEffectManager->playSound(buzzer_win10_sudo,
                                            sizeof(buzzer_win10_sudo) / sizeof(note_t));
    }

    // 修改VerifyPendingAction中的playSound调用，改为使用SoundEffectManager
    void update(openfsm::OpenFSM &fsm) const override
    {
        static auto last_error_time = std::chrono::steady_clock::now();
        auto *context = static_cast<InfusionStateMachine::FSMContext *>(fsm.getCustom());
        if (!context)
            return;

        // NFC卡读取与验证
        if (context->pn532Initialized)
        {
            uint8_t uidBuf[MIFARE_UID_MAX_LENGTH];
            int uidLen = PN532_ReadPassiveTarget(&context->pn532, uidBuf, PN532_MIFARE_ISO14443A, 1000);
            if (uidLen > 0)
            {
                if (isUidAllowed(uidBuf, uidLen))
                {
                    InfusionLogger::info("NFC验证成功，UID: {:02x}{:02x}{:02x}{:02x}", uidBuf[0], uidBuf[1], uidBuf[2], uidBuf[3]);
                    context->pumpState->state.store(VERIFIED);
                    // 播放验证成功音效
                    if (g_soundEffectManager)
                        g_soundEffectManager->playSound(buzzer_dji_startup,
                                                        sizeof(buzzer_dji_startup) / sizeof(note_t));
                    fsm.enterState(STATE_VERIFIED);
                    return;
                }
                else
                {

                    auto now = std::chrono::steady_clock::now();
                    if (now - last_error_time > std::chrono::seconds(5))
                    {
                        last_error_time = now;

                        if (g_soundEffectManager)
                            g_soundEffectManager->playSound(buzzer_error,
                                                            sizeof(buzzer_error) / sizeof(note_t));
                    }
                    // 记录未授权UID
                    InfusionLogger::warn("检测到未授权的UID: {:02x}{:02x}{:02x}{:02x}", uidBuf[0], uidBuf[1], uidBuf[2], uidBuf[3]);
                }
            }
        }
        // 检查状态是否被外部更新
        PumpControlState currentState = context->pumpState->state.load();
        if (currentState != VERIFY_PENDING)
        {
            // 状态被外部改变，进行状态转换
            switch (currentState)
            {
            case IDLE:
                fsm.enterState(STATE_IDLE);
                break;
            case VERIFIED:
                fsm.enterState(STATE_VERIFIED);
                break;
            case ERROR:
                fsm.enterState(STATE_ERROR);
                break;
            default:
                break;
            }
        }
    }

    void exit(openfsm::OpenFSM &fsm) const override
    {
        auto *context = static_cast<InfusionStateMachine::FSMContext *>(fsm.getCustom());
        if (!context)
            return;

        InfusionLogger::debug("正在离开验证待处理状态");
    }
};

// 已验证状态动作
class VerifiedAction : public openfsm::OpenFSMAction
{
public:
    VerifiedAction()
    {
        actionName_ = ACTION_VERIFIED;
    }

    void enter(openfsm::OpenFSM &fsm) const override
    {
        auto *context = static_cast<InfusionStateMachine::FSMContext *>(fsm.getCustom());
        if (!context)
            return;

        // 更新状态
        context->pumpState->state.store(VERIFIED);

        InfusionLogger::info("已进入已验证状态，可以进行输液操作");
    }

    void update(openfsm::OpenFSM &fsm) const override
    {
        auto *context = static_cast<InfusionStateMachine::FSMContext *>(fsm.getCustom());
        if (!context)
            return;

        // 检查状态是否被外部更新
        PumpControlState currentState = context->pumpState->state.load();
        if (currentState != VERIFIED)
        {
            // 状态被外部改变，进行状态转换
            switch (currentState)
            {
            case IDLE:
                fsm.enterState(STATE_IDLE);
                break;
            case PREPARING:
                fsm.enterState(STATE_PREPARING);
                break;
            case ERROR:
                fsm.enterState(STATE_ERROR);
                break;
            default:
                break;
            }
        }
    }

    void exit(openfsm::OpenFSM &fsm) const override
    {
        auto *context = static_cast<InfusionStateMachine::FSMContext *>(fsm.getCustom());
        if (!context)
            return;

        InfusionLogger::debug("正在离开已验证状态");
    }
};

// 准备状态动作
class PreparingAction : public openfsm::OpenFSMAction
{
public:
    PreparingAction()
    {
        actionName_ = ACTION_PREPARING;
    }

    // 进入准备状态
    void enter(openfsm::OpenFSM &fsm) const override
    {
        auto *context = static_cast<InfusionStateMachine::FSMContext *>(fsm.getCustom());
        if (!context)
            return;

        // 设置电机为正向低速运行，排空气
        context->motorDriver->setDirection(true); // 正向
        context->motorDriver->setSpeed(5.0);      // 低速

        // 初始化准备状态计时器（5秒）
        context->preparingTimer = 5000; // 5000毫秒
        context->lastUpdateTime = std::chrono::steady_clock::now();

        // 更新状态
        context->pumpState->state.store(PREPARING);
        context->pumpState->current_speed.store(5.0);

        InfusionLogger::info("已进入准备状态，电机低速正向运行");

        if (g_soundEffectManager)
            g_soundEffectManager->playSound(buzzer_chunriying,
                                            sizeof(buzzer_chunriying) / sizeof(note_t));
    }

    // 准备状态更新
    void update(openfsm::OpenFSM &fsm) const override
    {
        auto *context = static_cast<InfusionStateMachine::FSMContext *>(fsm.getCustom());
        if (!context)
            return;

        // 检查状态是否被外部更新
        PumpControlState currentState = context->pumpState->state.load();
        if (currentState != PREPARING)
        {
            // 状态被外部改变，进行状态转换
            switch (currentState)
            {
            case IDLE:
                fsm.enterState(STATE_IDLE);
                break;
            case EMERGENCY_STOP:
                fsm.enterState(STATE_EMERGENCY_STOP);
                break;
            case PAUSED:
                fsm.enterState(STATE_PAUSED);
                break;
            case INFUSING:
                fsm.enterState(STATE_INFUSING);
                break;
            case ERROR:
                fsm.enterState(STATE_ERROR);
                break;
            default:
                break;
            }
            return;
        }

        // 更新准备状态计时器
        auto now = std::chrono::steady_clock::now();
        int elapsedMs = static_cast<int>(std::chrono::duration_cast<std::chrono::milliseconds>(
                                             now - context->lastUpdateTime)
                                             .count());

        context->preparingTimer -= elapsedMs;
        context->lastUpdateTime = now;

        // 准备时间结束，转入输液状态
        if (context->preparingTimer <= 0)
        {
            InfusionLogger::info("准备阶段完成，转入输液状态");
            context->pumpState->state.store(INFUSING);
            fsm.enterState(STATE_INFUSING);
        }
    }

    // 离开准备状态
    void exit(openfsm::OpenFSM &fsm) const override
    {
        auto *context = static_cast<InfusionStateMachine::FSMContext *>(fsm.getCustom());
        if (!context)
            return;

        InfusionLogger::debug("正在离开准备状态");
    }
};

// 输液状态动作
class InfusingAction : public openfsm::OpenFSMAction
{
public:
    InfusingAction()
    {
        actionName_ = ACTION_INFUSING;
    }

    // 进入输液状态
    void enter(openfsm::OpenFSM &fsm) const override
    {
        auto *context = static_cast<InfusionStateMachine::FSMContext *>(fsm.getCustom());
        if (!context)
            return;

        // 根据目标流量设置电机转速
        double targetFlowRate = context->pumpParams->target_flow_rate.load();
        double targetRPM = context->pumpParams->target_rpm.load();

        // 设置电机方向和速度
        context->motorDriver->setDirection(context->pumpParams->direction.load());
        context->motorDriver->setSpeed(targetRPM);

        // 更新状态
        context->pumpState->state.store(INFUSING);
        context->pumpState->current_flow_rate.store(targetFlowRate);
        context->pumpState->current_speed.store(targetRPM);

        InfusionLogger::info("已进入输液状态，目标流量: {:.2f} ml/h, 目标转速: {:.2f} RPM",
                             targetFlowRate, targetRPM);
    }

    // 输液状态更新
    void update(openfsm::OpenFSM &fsm) const override
    {
        auto *context = static_cast<InfusionStateMachine::FSMContext *>(fsm.getCustom());
        if (!context)
            return;

        // 检查状态是否被外部更新
        PumpControlState currentState = context->pumpState->state.load();
        if (currentState != INFUSING)
        {
            // 状态被外部改变，进行状态转换
            switch (currentState)
            {
            case IDLE:
                fsm.enterState(STATE_IDLE);
                break;
            case EMERGENCY_STOP:
                fsm.enterState(STATE_EMERGENCY_STOP);
                break;
            case PAUSED:
                fsm.enterState(STATE_PAUSED);
                break;
            case ERROR:
                fsm.enterState(STATE_ERROR);
                break;
            default:
                break;
            }
            return;
        }

        // 持续输液，更新当前流量和进度
        double currentSpeed = context->motorDriver->getSpeed();
        context->pumpState->current_speed.store(currentSpeed);

        // 如果有液位检测更新，则更新进度信息
        double liquidLevel = context->pumpState->liquid_height.load();
        if (liquidLevel >= 0 && liquidLevel <= 100)
        {
            // 假设液位从100%到0%对应输液进度从0%到100%
            double progress = 100.0 - liquidLevel;
            context->pumpState->infusion_progress.store(progress);

            // 估算剩余时间（假设流量恒定）
            double targetFlowRate = context->pumpParams->target_flow_rate.load();
            if (targetFlowRate > 0)
            {
                // 假设总容量为100ml，计算剩余时间（小时）
                double remainingVolume = liquidLevel * 1.0; // 1.0 = 总容量/100
                double remainingHours = remainingVolume / targetFlowRate;
                int remainingSeconds = static_cast<int>(remainingHours * 3600);
                context->pumpState->remaining_time.store(remainingSeconds);
            }
        }
    }

    // 离开输液状态
    void exit(openfsm::OpenFSM &fsm) const override
    {
        auto *context = static_cast<InfusionStateMachine::FSMContext *>(fsm.getCustom());
        if (!context)
            return;

        InfusionLogger::debug("正在离开输液状态");
    }
};

// 暂停状态动作
class PausedAction : public openfsm::OpenFSMAction
{
public:
    PausedAction()
    {
        actionName_ = ACTION_PAUSED;
    }

    // 进入暂停状态
    void enter(openfsm::OpenFSM &fsm) const override
    {
        auto *context = static_cast<InfusionStateMachine::FSMContext *>(fsm.getCustom());
        if (!context)
            return;

        // 停止电机
        context->motorDriver->setSpeed(0);

        // 更新状态
        context->pumpState->state.store(PAUSED);
        context->pumpState->current_flow_rate.store(0.0);
        context->pumpState->current_speed.store(0.0);

        InfusionLogger::info("已进入暂停状态，电机已停止");

        if (g_soundEffectManager)
            g_soundEffectManager->playSound(buzzer_autopilot_disconnect,
                                            sizeof(buzzer_autopilot_disconnect) / sizeof(note_t));
    }

    // 暂停状态更新
    void update(openfsm::OpenFSM &fsm) const override
    {
        auto *context = static_cast<InfusionStateMachine::FSMContext *>(fsm.getCustom());
        if (!context)
            return;

        // 检查状态是否被外部更新
        PumpControlState currentState = context->pumpState->state.load();
        if (currentState != PAUSED)
        {
            // 状态被外部改变，进行状态转换
            switch (currentState)
            {
            case IDLE:
                fsm.enterState(STATE_IDLE);
                break;
            case INFUSING:
                fsm.enterState(STATE_INFUSING);
                break;
            case EMERGENCY_STOP:
                fsm.enterState(STATE_EMERGENCY_STOP);
                break;
            case ERROR:
                fsm.enterState(STATE_ERROR);
                break;
            default:
                break;
            }
        }
    }

    // 离开暂停状态
    void exit(openfsm::OpenFSM &fsm) const override
    {
        auto *context = static_cast<InfusionStateMachine::FSMContext *>(fsm.getCustom());
        if (!context)
            return;

        InfusionLogger::debug("正在离开暂停状态");
    }
};

// 紧急停止状态动作
class EmergencyStopAction : public openfsm::OpenFSMAction
{
public:
    EmergencyStopAction()
    {
        actionName_ = ACTION_EMERGENCY_STOP;
    }

    // 进入紧急停止状态
    void enter(openfsm::OpenFSM &fsm) const override
    {
        auto *context = static_cast<InfusionStateMachine::FSMContext *>(fsm.getCustom());
        if (!context)
            return;

        // 设置电机反转
        bool currentDirection = context->pumpState->direction.load();
        context->motorDriver->setDirection(!currentDirection);
        context->motorDriver->setSpeed(5.0); // 低速反转

        // 初始化紧急停止计时器（0.5秒）
        context->emergencyStopTimer = 500; // 500毫秒
        context->lastUpdateTime = std::chrono::steady_clock::now();

        // 更新状态
        context->pumpState->state.store(EMERGENCY_STOP);
        context->pumpState->current_flow_rate.store(0.0);
        context->pumpState->current_speed.store(5.0);

        InfusionLogger::warn("已进入紧急停止状态，电机低速反转");

        if (g_soundEffectManager)
            g_soundEffectManager->playSound(buzzer_autopilot_disconnect,
                                            sizeof(buzzer_autopilot_disconnect) / sizeof(note_t));
    }

    // 紧急停止状态更新
    void update(openfsm::OpenFSM &fsm) const override
    {
        auto *context = static_cast<InfusionStateMachine::FSMContext *>(fsm.getCustom());
        if (!context)
            return;

        // 检查状态是否被外部强制更新
        PumpControlState currentState = context->pumpState->state.load();
        if (currentState != EMERGENCY_STOP)
        {
            // 只允许转换到IDLE或ERROR状态
            if (currentState == IDLE || currentState == ERROR)
            {
                if (currentState == IDLE)
                {
                    fsm.enterState(STATE_IDLE);
                }
                else
                {
                    fsm.enterState(STATE_ERROR);
                }
            }
            else
            {
                // 不允许其他状态转换
                context->pumpState->state.store(EMERGENCY_STOP);
            }
            return;
        }

        // 更新紧急停止计时器
        auto now = std::chrono::steady_clock::now();
        int elapsedMs = static_cast<int>(std::chrono::duration_cast<std::chrono::milliseconds>(
                                             now - context->lastUpdateTime)
                                             .count());

        context->emergencyStopTimer -= elapsedMs;
        context->lastUpdateTime = now;

        // 反转时间结束，停止电机并转入空闲状态
        if (context->emergencyStopTimer <= 0)
        {
            context->motorDriver->setSpeed(0);
            InfusionLogger::warn("紧急停止完成，转入空闲状态");
            context->pumpState->state.store(IDLE);
            fsm.enterState(STATE_IDLE);
        }
    }

    // 离开紧急停止状态
    void exit(openfsm::OpenFSM &fsm) const override
    {
        auto *context = static_cast<InfusionStateMachine::FSMContext *>(fsm.getCustom());
        if (!context)
            return;

        InfusionLogger::debug("正在离开紧急停止状态");
    }
};

// 错误状态动作
class ErrorAction : public openfsm::OpenFSMAction
{
public:
    ErrorAction()
    {
        actionName_ = ACTION_ERROR;
    }

    // 进入错误状态
    void enter(openfsm::OpenFSM &fsm) const override
    {
        auto *context = static_cast<InfusionStateMachine::FSMContext *>(fsm.getCustom());
        if (!context)
            return;

        // 立即停止电机
        context->motorDriver->setSpeed(0);

        // 更新状态
        context->pumpState->state.store(ERROR);
        context->pumpState->current_flow_rate.store(0.0);
        context->pumpState->current_speed.store(0.0);

        InfusionLogger::error("已进入错误状态，需要手动重置系统");
    }

    // 错误状态更新
    void update(openfsm::OpenFSM &fsm) const override
    {
        auto *context = static_cast<InfusionStateMachine::FSMContext *>(fsm.getCustom());
        if (!context)
            return;

        // 检查状态是否被外部更新
        PumpControlState currentState = context->pumpState->state.load();
        if (currentState != ERROR)
        {
            // 错误状态只允许转换到IDLE状态
            if (currentState == IDLE)
            {
                fsm.enterState(STATE_IDLE);
            }
            else
            {
                // 不允许转换到其他状态
                context->pumpState->state.store(ERROR);
            }
        }
    }

    // 离开错误状态
    void exit(openfsm::OpenFSM &fsm) const override
    {
        auto *context = static_cast<InfusionStateMachine::FSMContext *>(fsm.getCustom());
        if (!context)
            return;

        InfusionLogger::debug("正在离开错误状态");
    }
};

// 输液状态机实现
InfusionStateMachine::InfusionStateMachine(MotorDriver &motorDriver, PumpParams &pumpParams, PumpState &pumpState)
    : motorDriver_(motorDriver), pumpParams_(pumpParams), pumpState_(pumpState)
{

    // 初始化状态机上下文
    fsmContext_.motorDriver = &motorDriver;
    fsmContext_.pumpParams = &pumpParams;
    fsmContext_.pumpState = &pumpState;
    fsmContext_.lastUpdateTime = std::chrono::steady_clock::now();
}

InfusionStateMachine::~InfusionStateMachine()
{
    // 清理状态机资源
    fsm_.reset();
}

bool InfusionStateMachine::initialize()
{
    try
    {
        // 创建状态机实例
        fsm_ = std::make_unique<openfsm::OpenFSM>();

        // 设置状态机自定义数据
        fsm_->setCustom(&fsmContext_);

        // 创建所有状态
        auto idleState = new openfsm::OpenFSMState(static_cast<int>(IDLE), STATE_IDLE);
        auto verifyPendingState = new openfsm::OpenFSMState(static_cast<int>(VERIFY_PENDING), STATE_VERIFY_PENDING);
        auto verifiedState = new openfsm::OpenFSMState(static_cast<int>(VERIFIED), STATE_VERIFIED);
        auto preparingState = new openfsm::OpenFSMState(static_cast<int>(PREPARING), STATE_PREPARING);
        auto infusingState = new openfsm::OpenFSMState(static_cast<int>(INFUSING), STATE_INFUSING);
        auto pausedState = new openfsm::OpenFSMState(static_cast<int>(PAUSED), STATE_PAUSED);
        auto emergencyStopState = new openfsm::OpenFSMState(static_cast<int>(EMERGENCY_STOP), STATE_EMERGENCY_STOP);
        auto errorState = new openfsm::OpenFSMState(static_cast<int>(ERROR), STATE_ERROR);

        // 添加动作到各个状态
        idleState->addAction(new IdleAction());
        verifyPendingState->addAction(new VerifyPendingAction());
        verifiedState->addAction(new VerifiedAction());
        preparingState->addAction(new PreparingAction());
        infusingState->addAction(new InfusingAction());
        pausedState->addAction(new PausedAction());
        emergencyStopState->addAction(new EmergencyStopAction());
        errorState->addAction(new ErrorAction());

        // 将状态添加到状态机
        fsm_->addState(idleState);
        fsm_->addState(verifyPendingState);
        fsm_->addState(verifiedState);
        fsm_->addState(preparingState);
        fsm_->addState(infusingState);
        fsm_->addState(pausedState);
        fsm_->addState(emergencyStopState);
        fsm_->addState(errorState);

        // 设置初始状态
        fsm_->enterState(STATE_IDLE);

        InfusionLogger::info("输液状态机初始化完成");
        return true;
    }
    catch (const std::exception &e)
    {
        InfusionLogger::error("初始化输液状态机时出错: {}", e.what());
        return false;
    }
}

void InfusionStateMachine::update()
{
    if (fsm_)
    {
        fsm_->update();
    }
}

void InfusionStateMachine::setState(PumpControlState state)
{
    if (pumpState_.state.load() != state)
    {
        // 验证状态转换是否合法
        if (!isValidStateTransition(pumpState_.state.load(), state))
        {
            InfusionLogger::warn("不合法的状态转换: 从 {} 到 {}",
                                 static_cast<int>(pumpState_.state.load()), static_cast<int>(state));
            return;
        }

        // 更新状态
        pumpState_.state.store(state);

        // 如果状态机已初始化，直接在状态机中处理状态转换
        if (fsm_)
        {
            switch (state)
            {
            case IDLE:
                fsm_->enterState(STATE_IDLE);
                break;
            case VERIFY_PENDING:
                fsm_->enterState(STATE_VERIFY_PENDING);
                break;
            case VERIFIED:
                fsm_->enterState(STATE_VERIFIED);
                break;
            case PREPARING:
                fsm_->enterState(STATE_PREPARING);
                break;
            case INFUSING:
                fsm_->enterState(STATE_INFUSING);
                break;
            case PAUSED:
                fsm_->enterState(STATE_PAUSED);
                break;
            case EMERGENCY_STOP:
                fsm_->enterState(STATE_EMERGENCY_STOP);
                break;
            case ERROR:
                fsm_->enterState(STATE_ERROR);
                break;
            }
        }
    }
}

bool InfusionStateMachine::isValidStateTransition(PumpControlState from, PumpControlState to) const
{
    // 定义合法的状态转换规则
    switch (from)
    {
    case IDLE:
        // 从IDLE可以转换到的状态
        return (to == VERIFY_PENDING || to == PREPARING || to == ERROR);

    case VERIFY_PENDING:
        // 从VERIFY_PENDING可以转换到的状态
        return (to == IDLE || to == VERIFIED || to == ERROR);

    case VERIFIED:
        // 从VERIFIED可以转换到的状态
        return (to == IDLE || to == PREPARING || to == ERROR);

    case PREPARING:
        // 从PREPARING可以转换到的状态
        return (to == IDLE || to == INFUSING || to == PAUSED || to == EMERGENCY_STOP || to == ERROR);

    case INFUSING:
        // 从INFUSING可以转换到的状态
        return (to == IDLE || to == PAUSED || to == EMERGENCY_STOP || to == ERROR);

    case PAUSED:
        // 从PAUSED可以转换到的状态
        return (to == IDLE || to == INFUSING || to == EMERGENCY_STOP || to == ERROR);

    case EMERGENCY_STOP:
        // 从EMERGENCY_STOP可以转换到的状态
        return (to == IDLE || to == ERROR);

    case ERROR:
        // 从ERROR只能转换到IDLE状态
        return (to == IDLE);

    default:
        return false;
    }
}

PumpControlState InfusionStateMachine::getState() const
{
    return pumpState_.state.load();
}
