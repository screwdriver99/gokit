#ifndef MAINWINDOW_H
#define MAINWINDOW_H

#include <blewrapper/central.h>
#include <ultragui/types.h>

#include <QMainWindow>
#include <QTimer>

QT_BEGIN_NAMESPACE
namespace Ui
{
    class MainWindow;
}
QT_END_NAMESPACE

enum DeviceState : uint8_t
{
    DS_MM_Idle        = 0,
    DS_MM_DCVoltage   = 1,
    DS_MM_ACVoltage   = 2,
    DS_MM_DCCUrrent   = 3,
    DS_MM_ACCurrent   = 4,
    DS_MM_Resistance  = 5,
    DS_MM_Diode       = 6,
    DS_MM_Continuity  = 7,
    DS_MM_Temperature = 8,
    DS_DSO            = 9,
    DS_Datalogger     = 10,
};

enum MultimeterMode : uint8_t
{
    MM_IDLE        = 0,
    MM_DCVoltage   = 1,
    MM_ACVoltage   = 2,
    MM_DCCurrent   = 3,
    MM_ACCurrent   = 4,
    MM_Resistance  = 5,
    MM_Diode       = 6,
    MM_Continuity  = 7,
    MM_Temperature = 8,
};

enum ModeSwitchPosition : uint8_t
{
    MSP_Voltage = 0,
    MSP_Mixed   = 1,
    MSP_Current = 2,
};

enum ButtonAction : uint8_t
{
    BA_Release   = 0,
    BA_Pressed   = 1,
    BA_LongPress = 2,
};

enum DSOCommand : uint8_t
{
    DSOC_FreeRunning = 0,
    DSOC_RisingEdge  = 1,
    DSOC_FallingEdge = 2,
    DSOC_Resend      = 3,
    DSOC_Spare0      = 4,  // unknown
    DSOC_Continuos   = 5,  // undocumented
};

enum DSOOpMode : uint8_t
{
    DOM_Idle = 0,
    DOM_VDC  = 1,
    DOM_VAC  = 2,
    DOM_ADC  = 3,
    DOM_AAC  = 4,
};

enum DSOStatus : uint8_t
{
    DS_Done     = 0,
    DS_Sampling = 1,
    DS_Error    = 255,
};

#pragma pack(push, 1)
struct DeviceData
{
    uint8_t fwMaj;
    uint8_t fwMin;
    uint16_t maxVoltage;       // In V
    uint16_t maxCurrent;       // In A
    uint16_t maxResistance;    // In KOhm
    uint16_t maxSamplingRate;  // in KHz
    uint16_t maxBufferSize;
    uint16_t reserved;
    uint8_t macAddr[6];
};

struct DeviceStatus
{
    DeviceState state;
    float batteryVoltage;  //[0 - 3.3v]

    // undocumented
    uint8_t spare0;
    ModeSwitchPosition modeswitch;
    uint8_t spare1;
};

struct MMSettings
{
    MultimeterMode mode;
    uint8_t range;
    uint32_t updateInterval;
};

struct MMReading
{
    uint8_t status;
    float value;
    MultimeterMode mode;
    uint8_t range;
};

struct DeviceButton
{
    uint8_t spare;
    ButtonAction button;
};

struct DSOSettings
{
    DSOCommand command;
    float trigger;
    DSOOpMode mode;
    uint8_t range;
    uint32_t window;
    uint16_t samples;  // 1~8192
};

struct DSOMetadata
{
    DSOStatus status;
    float scale;
    DSOOpMode mode;
    uint8_t range;
    uint32_t window;
    uint16_t samples;
    uint32_t samplingRate;

    // undocumented and unknown data:
    uint8_t spare0;
    uint8_t spare1;
    uint8_t spare2;
    uint8_t spare3;
    uint8_t spare4;
};

struct DSOReading
{
    int16_t data[88];  // doc says 10 -_-
};

#pragma pack(pop)

class MainWindow : public QMainWindow, private blew::Central
{
    Q_OBJECT

   public:
    MainWindow(QWidget* parent = nullptr);
    virtual ~MainWindow();

   private:
    Ui::MainWindow* ui;
    blew::ble_peripheral m_peripheral;
    QTimer m_scanTimer, m_mmrxTimer, m_dsorxTimer;

    enum GUIDevMode
    {
        GDM_Multimeter,
        GDM_Oscilloscope,
        GDM_Datalogger,
    };

    float m_dsoScale;

    DSOCommand m_dsoCmd;

    MultimeterMode _currentMMMode();
    uint8_t _currentMMRange();
    uint8_t _currentDSORange();
    DSOCommand _currentDSOCommand();
    DSOOpMode _currentDSOMode();

    QString _modeswToStr(ModeSwitchPosition mode);

    void _updateDevData(const DeviceData& data);
    void _updateDevStatus(const DeviceStatus& status);

    void _updateDeviceMMMode();
    void _updateDeviceDSOMode(bool stop = false);

    void _setupMMModeSelector(ModeSwitchPosition sw);
    void _setupMMRangeSelector(MultimeterMode mode);
    void _setupDSORangeSelector(DSOOpMode mode);
    void _setupDSOOscilloscope(const DSOMetadata& metadata);

    static QString _mmrangeToStr(uint8_t range, MultimeterMode mode);
    static QString _mmmodeToStr(MultimeterMode mode);
    static float _dsorangeToMax(DSOOpMode mode, uint8_t range);
    static QString _dsorangeToStr(DSOOpMode mode, uint8_t range);

    void _updateMMLeds(MultimeterMode mode, uint8_t status);

    void _dsoReading(const DSOReading& data, uint32_t size);
    void _dsoMetadata(const DSOMetadata& metadata);

    virtual void centralStateChanged(blew::CentralState newState) override;
    virtual void peripheralDiscovered(blew::ble_peripheral peripheral) override;
    virtual void peripheralConnected(blew::ble_peripheral peripheral) override;
    virtual void peripheralDisconnected(blew::ble_peripheral peripheral) override;
    virtual void peripheralUpdatedRSSI(blew::ble_peripheral peripheral) override;

    virtual void servicesDiscovered(blew::ble_peripheral peripheral) override;
    virtual void includedServicesDiscovered(blew::ble_service peripheral) override;

    virtual void charsDiscovered(blew::ble_service service) override;
    virtual void charValueUpdated(blew::ble_char characteristic) override;
    virtual void charValueWritten(blew::ble_char characteristic) override;
    virtual void charUpdatedSubscribeStatus(blew::ble_char characteristic, bool ok) override;

   private slots:
    void _onScanButtonClick();
    void _onScanTimerTimeout();
    void _onMMRxTimerTimeout();
    void _onDSORxTimerTimeout();
    void _deviceSelected(const gui::UltraEntry*);
    void _onConnectButtonClick();

    void _onDeviceModeChange(int32_t id, void* p);

    void _onMultimeterModeChange(int32_t id, void* p);
    void _onMultimeterRangeSelectorPress(const gui::UltraEntry*);

    void _onDSOModeChange(int32_t id, void* p);
    void _onDSORangeSelectorPress(const gui::UltraEntry*);
    void _onDSOMeasureChange(int32_t id, void* p);

    void _onTorchButtonChange(bool);
    void _onDsoTriggerButtonChange(bool);
};
#endif  // MAINWINDOW_H
